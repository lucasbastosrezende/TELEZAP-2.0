// ============================================================================
//  TocaChat WebRTC — PeerJS (Mesh self-hosted) + TURN
// ============================================================================

let peer = null;
let activeCalls = {}; // map de participantId => callObject
let localStream = null;
let isJoined = false;

// FIX: fila para chamadas recebidas antes da mídia local estar pronta
let pendingCalls = [];

let isInCall = false;
window.isInCall = false;
let isCameraOn = false;
window.isCameraOn = false;
let isMuted = false;

let callSourceId = null;
window.callSourceId = null;

// Indicador "em chamada" na lista de participantes (Task 1)
const usersInCall = new Set();
window.usersInCall = usersInCall;

// FIX: estado de reconexão PeerJS — backoff exponencial e limite de tentativas
let reconnectAttempts = 0;
const MAX_RECONNECT_ATTEMPTS = 5;
let reconnectTimer = null;
// FIX: diferencia "nunca conectou" (falha rápida) de "desconectou em chamada" (backoff)
let peerHadOpenOnce = false;

// Ringtone setup
let ringtoneCtx = null;
let ringtoneInterval = null;
let incomingCallTimeout = null;

// ══════════════════════════════════════════════
//  Signaling (Para tocar aviso de chamada recebida)
// ══════════════════════════════════════════════
async function sendCallSignal(destId, tipo, dados, convId = null) {
    const cid = convId || window.callSourceId;
    if (!cid) return;
    try {
        await api('/api/calls/signal', {
            method: 'POST',
            body: { conversa_id: cid, destinatario_id: destId, tipo: tipo, dados: dados }
        });
    } catch (e) {
        console.error('[Call] Signal send error:', e);
    }
}

window.handleIncomingSignal = async function (sinal) {
    const { remetente_id, tipo, dados, conversa_id } = sinal;
    console.log('[Call] Módulo de Alerta WebRTC Recebeu Sinal:', tipo, 'da conversa', conversa_id);

    const callConv = (window.conversas || []).find(c => c.id == conversa_id);
    if (!callConv) return;
    const caller = callConv.membros?.find(m => m.id == remetente_id);

    if (tipo === 'join') {
        if (!isInCall) {
            if (caller) showIncomingCallAlert(callConv, caller, dados);
        }
    } else if (tipo === 'leave') {
        dismissIncomingCall();
        if (isInCall) removeRemoteParticipant(remetente_id);
    } else if (tipo === 'decline') {
        showToast(`${caller?.nome || 'Usuário'} recusou a chamada.`, 'info');
    } else if (tipo === 'state') {
        updateRemoteParticipantState(remetente_id, dados);
    } else if (tipo === 'in_call') {
        // Task 1: indicador de participante em chamada na sidebar
        markParticipantInCall(dados.userId, true);
    } else if (tipo === 'call_ended') {
        markParticipantInCall(dados.userId, false);
    }
}

function updateRemoteParticipantState(participantId, state) {
    const videoNode = document.getElementById(`video-${participantId}`);
    const avatarNode = document.getElementById(`avatar-${participantId}`);
    if (!videoNode || !avatarNode || state.isCameraOn === undefined) return;

    if (state.isCameraOn) {
        videoNode.classList.remove('hidden');
        avatarNode.classList.add('hidden');
    } else {
        videoNode.classList.add('hidden');
        avatarNode.classList.remove('hidden');
    }
}

function broadcastStateChange() {
    if (!isInCall || !window.conversaAtual) return;
    const others = window.conversaAtual.membros.filter(m => m.id !== currentUser.id);
    for (const m of others) {
        sendCallSignal(m.id, 'state', { isMuted: isMuted, isCameraOn: isCameraOn }, callSourceId);
    }
}

// ══════════════════════════════════════════════
//  Task 1: Indicador "Em chamada" na lista de participantes
// ══════════════════════════════════════════════
const CALL_BADGE_SVG = '<path d="M22 16.92v3a2 2 0 0 1-2.18 2 19.79 19.79 0 0 1-8.63-3.07 19.5 19.5 0 0 1-6-6 19.79 19.79 0 0 1-3.07-8.67A2 2 0 0 1 4.11 2h3a2 2 0 0 1 2 1.72c.127.96.361 1.903.7 2.81a2 2 0 0 1-.45 2.11L8.09 9.91a16 16 0 0 0 6 6l1.27-1.27a2 2 0 0 1 2.11-.45c.907.339 1.85.573 2.81.7A2 2 0 0 1 22 16.92z"/>';

function markParticipantInCall(userId, isActive) {
    if (userId == null || userId === undefined) return;
    const id = String(userId);
    const list = document.getElementById('participantsList');
    if (!list) return;
    // Busca por data-member-id (sidebar) — defensivo: falha silenciosa se não achar
    let el = list.querySelector(`[data-member-id="${id}"]`);
    if (!el) return;

    if (isActive) {
        usersInCall.add(id);
        el.classList.add('member-in-call');
        let badge = document.getElementById(`call-badge-${id}`);
        if (!badge) {
            badge = document.createElement('span');
            badge.className = 'call-status-badge';
            badge.id = `call-badge-${id}`;
            badge.innerHTML = `<svg width="12" height="12" viewBox="0 0 24 24" fill="currentColor">${CALL_BADGE_SVG}</svg> Em chamada`;
            const info = el.querySelector('.participant-info');
            if (info) info.appendChild(badge);
        }
        list.insertBefore(el, list.firstChild);
    } else {
        usersInCall.delete(id);
        el.classList.remove('member-in-call');
        const badge = document.getElementById(`call-badge-${id}`);
        if (badge) badge.remove();
        // Restaurar ordem natural: reordena pela lógica da lista (ordem alfabética)
        if (typeof window.renderParticipantsSidebar === 'function' && window.conversaAtual) {
            window.renderParticipantsSidebar(window.conversaAtual);
            if (typeof window.reapplyParticipantsInCall === 'function') {
                window.reapplyParticipantsInCall();
            }
        }
    }
}

// Reaplica badge/class para todos em usersInCall após re-render da lista (ex.: troca de conversa)
window.reapplyParticipantsInCall = function reapplyParticipantsInCall() {
    const list = document.getElementById('participantsList');
    if (!list) return;
    usersInCall.forEach((id) => {
        const el = list.querySelector(`[data-member-id="${id}"]`);
        if (!el) return;
        el.classList.add('member-in-call');
        let badge = document.getElementById(`call-badge-${id}`);
        if (!badge) {
            badge = document.createElement('span');
            badge.className = 'call-status-badge';
            badge.id = `call-badge-${id}`;
            badge.innerHTML = `<svg width="12" height="12" viewBox="0 0 24 24" fill="currentColor">${CALL_BADGE_SVG}</svg> Em chamada`;
            const info = el.querySelector('.participant-info');
            if (info) info.appendChild(badge);
        }
        list.insertBefore(el, list.firstChild);
    });
};

// ══════════════════════════════════════════════
//  PeerJS Init & Mesh Flow (FIX: backoff + max retry + pendingCalls)
// ══════════════════════════════════════════════
function initPeer() {
    // FIX: verifica estado real antes de reusar o peer
    if (peer && !peer.destroyed && peer.open) return;
    if (peer && !peer.destroyed) {
        peer.destroy();
    }
    peer = null;
    reconnectAttempts = 0;
    peerHadOpenOnce = false;
    _createPeer();
}

function _createPeer() {
    // A Mágica do 4G: Servidores TURN e STUN Públicos Grátis
    const customConfig = {
        iceServers: [
            { urls: 'stun:stun.l.google.com:19302' },
            { urls: 'stun:stun1.l.google.com:19302' },
            {
                urls: 'turn:global.relay.metered.ca:80',
                username: 'openrelayproject',
                credential: 'openrelayproject'
            },
            {
                urls: 'turn:global.relay.metered.ca:443',
                username: 'openrelayproject',
                credential: 'openrelayproject'
            },
            {
                urls: 'turn:global.relay.metered.ca:443?transport=tcp',
                username: 'openrelayproject',
                credential: 'openrelayproject'
            }
        ]
    };

    peer = new Peer(String(currentUser.id), {
        host: window.location.hostname,
        port: 9000,
        path: '/myapp',
        secure: window.location.protocol === 'https:',
        config: customConfig,
        debug: 1,
        pingInterval: 5000 // FIX: keep-alive para evitar timeout silencioso do WS
    });

    peer.on('open', (id) => {
        console.log('[PeerJS] Conectado. Meu ID:', id);
        peerHadOpenOnce = true; // FIX: marca que já conectou alguma vez (reconexão faz sentido)
        reconnectAttempts = 0;
        if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
    });

    peer.on('disconnected', () => {
        if (!isInCall) {
            console.warn('[PeerJS] Desconectado. Não está em chamada, não reconectando.');
            return;
        }
        // FIX: falha rápida na primeira conexão — servidor inacessível (ex.: porta 9000 fechada / Tailscale)
        if (!peerHadOpenOnce) {
            console.error('[PeerJS] Servidor de sinalização inacessível (conexão inicial falhou).');
            if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
            showToast('Servidor de chamadas indisponível (porta 9000). Verifique se o PeerJS está em execução.', 'error');
            endCall(false);
            return;
        }
        // FIX: backoff exponencial só quando já tinha conectado antes (reconexão)
        if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
            console.error('[PeerJS] Máximo de reconexões atingido. Encerrando chamada.');
            showToast('Conexão com o servidor perdida. A chamada foi encerrada.', 'error');
            endCall(false);
            return;
        }
        const delay = Math.min(1000 * Math.pow(2, reconnectAttempts), 30000);
        reconnectAttempts++;
        console.warn(`[PeerJS] Desconectado. Tentativa ${reconnectAttempts}/${MAX_RECONNECT_ATTEMPTS} em ${delay}ms...`);
        reconnectTimer = setTimeout(() => {
            if (peer && !peer.destroyed) {
                peer.reconnect();
            } else {
                _createPeer();
            }
        }, delay);
    });

    peer.on('call', (call) => {
        console.log('[PeerJS] Recebendo chamada Mesh de:', call.peer);
        if (isInCall && localStream) {
            call.answer(localStream);
            handleCallStream(call);
            if (call.metadata) updateRemoteParticipantState(call.peer, call.metadata);
        } else if (isInCall && !localStream) {
            // FIX: fila de chamadas enquanto a mídia local ainda não está pronta
            pendingCalls.push(call);
        } else {
            call.close();
        }
    });

    peer.on('error', (err) => {
        if (err.type === 'disconnected' || err.type === 'network') {
            // FIX: se nunca conectou, evita loop de reconexão — disconnected já vai encerrar a chamada
            if (!peerHadOpenOnce && isInCall) {
                console.warn('[PeerJS] Servidor inacessível (conexão inicial):', err.message);
            } else {
                console.warn('[PeerJS] Erro de rede (esperado durante reconexão):', err.message);
            }
        } else {
            console.error('[PeerJS] Erro crítico:', err);
        }
    });
}

function handleCallStream(call) {
    if (activeCalls[call.peer]) {
        activeCalls[call.peer].close(); // limpa fantasmas
    }
    activeCalls[call.peer] = call;

    call.on('stream', (remoteStream) => {
        console.log('[PeerJS] Stream recebida ativamente do peer:', call.peer);
        renderRemoteParticipant(call.peer, remoteStream, call.metadata);
    });

    call.on('close', () => {
        removeRemoteParticipant(call.peer);
    });
    call.on('error', (err) => {
        console.error('[PeerJS] Erro de rede na chamada com', call.peer, err);
        removeRemoteParticipant(call.peer);
    });
}

// ══════════════════════════════════════════════
//  UI Controls & Mídia Nativa
// ══════════════════════════════════════════════
async function getLocalMedia() {
    if (typeof populateMicrophones === 'function') {
        populateMicrophones();
    }

    try {
        const constraints = { audio: true, video: isCameraOn };
        if (!isCameraOn) constraints.video = false;

        localStream = await navigator.mediaDevices.getUserMedia(constraints);

        const localVid = document.getElementById('localVideo');
        if (localVid) {
            localVid.srcObject = localStream;
        }

        if (isMuted) {
            localStream.getAudioTracks().forEach(t => t.enabled = false);
        }

        updateLocalVideo();
        // FIX: atende chamadas que chegaram enquanto a mídia local não estava pronta
        if (pendingCalls.length > 0) {
            pendingCalls.forEach((pendingCall) => {
                pendingCall.answer(localStream);
                handleCallStream(pendingCall);
            });
            pendingCalls = [];
        }
        return true;
    } catch (err) {
        console.error('[PeerJS/WebRTC] Falha ao capturar media do usuario:', err);
        if (isCameraOn) {
            isCameraOn = false;
            window.isCameraOn = false;
            return getLocalMedia(); // fallback sem video se deu erro
        }
        return false;
    }
}

function showCallUI() {
    document.getElementById('chatMessages').classList.add('hidden');
    document.getElementById('chatInputArea').classList.add('hidden');

    const inlineView = document.getElementById('callInlineView');
    if (inlineView) inlineView.classList.remove('hidden');

    const grid = document.getElementById('remoteVideosGrid');
    if (grid) grid.innerHTML = '';

    updateControlsUI();
}

function hideCallUI() {
    const inlineView = document.getElementById('callInlineView');
    if (inlineView) inlineView.classList.add('hidden');

    document.getElementById('chatMessages').classList.remove('hidden');
    document.getElementById('chatInputArea').classList.remove('hidden');

    const grid = document.getElementById('remoteVideosGrid');
    if (grid) grid.innerHTML = '';
}

function showCallView() {
    if (!isInCall) return;
    document.getElementById('chatMessages').classList.add('hidden');
    document.getElementById('chatInputArea').classList.add('hidden');
    document.getElementById('subtopicsBar').classList.add('hidden');
    document.getElementById('callActiveBar').classList.add('hidden');

    const inlineView = document.getElementById('callInlineView');
    if (inlineView) inlineView.classList.remove('hidden');
}

function hideCallView() {
    if (!isInCall) return;
    const inlineView = document.getElementById('callInlineView');
    if (inlineView) inlineView.classList.add('hidden');

    document.getElementById('chatMessages').classList.remove('hidden');
    document.getElementById('chatInputArea').classList.remove('hidden');

    if (window.conversaAtual && window.conversaAtual.tipo === 'grupo') {
        document.getElementById('subtopicsBar').classList.remove('hidden');
    }

    document.getElementById('callActiveBar').classList.remove('hidden');
    setTimeout(() => {
        const msgs = document.getElementById('chatMessages');
        if (msgs) msgs.scrollTop = msgs.scrollHeight;
    }, 100);
}

function updateControlsUI() {
    const btnMute = document.getElementById('btnMuteMic');
    const btnCam = document.getElementById('btnToggleCamera');

    if (btnMute) {
        btnMute.classList.toggle('off', isMuted);
        btnMute.innerHTML = isMuted
            ? `<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="1" y1="1" x2="23" y2="23"/><path d="M9 9v3a3 3 0 0 0 5.12 2.12M15 9.34V4a3 3 0 0 0-5.94-.6"/><path d="M17 16.95A7 7 0 0 1 5 12v-2m14 0v2a7 7 0 0 1-.11 1.23"/><line x1="12" y1="19" x2="12" y2="23"/><line x1="8" y1="23" x2="16" y2="23"/></svg>`
            : `<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 1a3 3 0 0 0-3 3v8a3 3 0 0 0 6 0V4a3 3 0 0 0-3-3z"/><path d="M19 10v2a7 7 0 0 1-14 0v-2"/><line x1="12" y1="19" x2="12" y2="23"/><line x1="8" y1="23" x2="16" y2="23"/></svg>`;
    }

    if (btnCam) {
        btnCam.classList.toggle('off', !isCameraOn);
        btnCam.innerHTML = isCameraOn
            ? `<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M23 7l-7 5 7 5V7z"/><rect x="1" y="5" width="15" height="14" rx="2" ry="2"/></svg>`
            : `<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M16.5 7.5l6.5-4v17l-6.5-4"/><path d="M2 5h11a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H2z"/><line x1="1" y1="1" x2="23" y2="23"/></svg>`;
    }
}

function updateLocalVideo() {
    const vid = document.getElementById('localVideo');
    const avatar = document.getElementById('localVideoAvatar');
    const initial = document.getElementById('localAvatarContent');

    if (!vid || !avatar) return;

    if (isCameraOn) {
        vid.classList.remove('hidden');
        avatar.classList.add('hidden');
    } else {
        vid.classList.add('hidden');
        avatar.classList.remove('hidden');

        if (currentUser) {
            if (currentUser.foto) {
                avatar.innerHTML = `<img src="${currentUser.foto}" alt="" style="width:100%; height:100%; object-fit:cover; border-radius:50%;">`;
            } else {
                avatar.innerHTML = `<video autoplay loop muted playsinline class="default-avatar-vid" style="width:100%; height:100%; object-fit:cover; border-radius:50%;"><source src="/static/images/Criação_de_Animação_Abstrata_Anime.mp4" type="video/mp4"></video>`;
            }
        }
    }
}

function updateCallStatusText(text) {
    const el = document.getElementById('callStatus');
    if (el) el.textContent = text;
}

// ══════════════════════════════════════════════
//  Grid Manager (UI Customizada)
// ══════════════════════════════════════════════
function updateGridLayout() {
    const grid = document.getElementById('remoteVideosGrid');
    if (!grid) return;
    const count = grid.children.length;

    grid.className = 'call-participants-grid';
    if (count === 1) grid.classList.add('grid-1');
    else if (count === 2) grid.classList.add('grid-2');
    else if (count > 2) grid.classList.add('grid-n');
}

function renderRemoteParticipant(participantId, stream, metadata = null) {
    const grid = document.getElementById('remoteVideosGrid');
    if (!grid) return;

    let container = document.getElementById(`participant-${participantId}`);

    if (!container) {
        container = document.createElement('div');
        container.className = 'remote-participant';
        container.id = `participant-${participantId}`;

        let fallbackName = 'Usuário';
        if (window.conversaAtual) {
             const member = window.conversaAtual.membros.find(m => String(m.id) === String(participantId));
             if (member) fallbackName = member.nome;
        }

        const initial = fallbackName.charAt(0).toUpperCase();

        container.innerHTML = `
            <video autoplay playsinline class="hidden" id="video-${participantId}"></video>
            <div class="remote-avatar" id="avatar-${participantId}">
                <video autoplay loop muted playsinline class="default-avatar-vid" style="width:100%; height:100%; object-fit:cover; border-radius:50%;"><source src="/static/images/Criação_de_Animação_Abstrata_Anime.mp4" type="video/mp4"></video>
            </div>
            <div class="remote-label">${fallbackName}</div>
        `;
        grid.appendChild(container);
    }

    const videoNode = document.getElementById(`video-${participantId}`);
    const avatarNode = document.getElementById(`avatar-${participantId}`);

    if (videoNode && stream) {
        videoNode.srcObject = stream;
        videoNode.onloadedmetadata = () => videoNode.play().catch(e=>console.warn(e));
        
        videoNode.classList.remove('hidden');
        if (avatarNode) avatarNode.classList.add('hidden');
        
        if (metadata && metadata.isCameraOn === false) {
            videoNode.classList.add('hidden');
            if (avatarNode) avatarNode.classList.remove('hidden');
        }

        stream.getVideoTracks().forEach(track => {
            track.onmute = () => {
                videoNode.classList.add('hidden');
                if (avatarNode) avatarNode.classList.remove('hidden');
            };
            track.onunmute = () => {
                videoNode.classList.remove('hidden');
                if (avatarNode) avatarNode.classList.add('hidden');
            };
        });
    }

    updateGridLayout();
}

function removeRemoteParticipant(participantId) {
    const container = document.getElementById(`participant-${participantId}`);
    if (container) {
        container.remove();
        updateGridLayout();
    }
    if (activeCalls[participantId]) {
        delete activeCalls[participantId];
    }
}

// ══════════════════════════════════════════════
//  Main Actions (Ligar, Mutar, Sair)
// ══════════════════════════════════════════════
async function joinCall() {
    if (isInCall) return;

    isInCall = true;
    window.isInCall = true;
    isMuted = false;
    isCameraOn = false; window.isCameraOn = false;
    callSourceId = window.conversaAtual ? window.conversaAtual.id : null;
    window.callSourceId = callSourceId;

    if (!peer) initPeer();

    showCallUI();
    updateCallStatusText('Conectando dispositivos...');

    const hasMedia = await getLocalMedia();
    if (!hasMedia) {
        showToast('Não foi possível acessar a câmera e microfone.', 'error');
        endCall(false);
        return;
    }

    updateCallStatusText('Conectado à nuvem p2p..');
    isJoined = true;

    // Task 1: indicador local "em chamada" na lista de participantes
    markParticipantInCall(currentUser.id, true);

    const conv = window.conversaAtual;
    if (conv) {
        const others = conv.membros.filter(m => m.id !== currentUser.id);

        // Task 1: broadcast in_call para todos os membros
        for (const m of others) {
            await sendCallSignal(m.id, 'in_call', { userId: currentUser.id }, callSourceId);
        }

        // 1. Avisa os outros que estou ligando (Sinal Visual)
        for (const m of others) {
            await sendCallSignal(m.id, 'join', {}, callSourceId);
        }

        // 2. Transmite meu vídeo para os outros na mesma porta do servidor WebRTC
        for (const m of others) {
            const call = peer.call(String(m.id), localStream, {
                metadata: { isMuted: isMuted, isCameraOn: isCameraOn }
            });
            if (call) {
                handleCallStream(call);
            }
        }
    }
    if (window.performSync) window.performSync();
}

async function endCall(sendSignal = true) {
    if (sendSignal && isInCall && window.conversaAtual) {
        const others = window.conversaAtual.membros.filter(m => m.id !== currentUser.id);
        // Task 1: broadcast call_ended para todos os membros
        for (const m of others) {
            await sendCallSignal(m.id, 'call_ended', { userId: currentUser.id }, callSourceId);
        }
        for (const m of others) {
            await sendCallSignal(m.id, 'leave', {}, callSourceId);
        }
    }

    // Task 1: remove indicador local "em chamada"
    markParticipantInCall(currentUser.id, false);

    // FIX: limpa estado de reconexão e fila de chamadas pendentes
    reconnectAttempts = 0;
    if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
    pendingCalls = [];

    if (localStream) {
        localStream.getTracks().forEach(t => t.stop());
        localStream = null;
    }

    Object.values(activeCalls).forEach(call => call.close());
    activeCalls = {};

    isInCall = false;
    window.isInCall = false;
    callSourceId = null;
    window.callSourceId = null;
    isJoined = false;

    hideCallUI();

    // FIX: NÃO destruir o peer no endCall — apenas fechar as chamadas ativas; reutilizado na próxima chamada
    if (window.performSync) window.performSync();
}

function toggleMute() {
    isMuted = !isMuted;
    if (localStream) {
        localStream.getAudioTracks().forEach(t => t.enabled = !isMuted);
    }
    updateControlsUI();
    updateLocalVideo();
    broadcastStateChange();
}

async function toggleCamera() {
    isCameraOn = !isCameraOn;
    window.isCameraOn = isCameraOn;

    const btnCam = document.getElementById('btnToggleCamera');
    if (btnCam) btnCam.classList.add('loading');

    try {
        if (localStream) {
            localStream.getVideoTracks().forEach(t => { t.stop(); localStream.removeTrack(t); });
        }

        if (isCameraOn) {
            const newStream = await navigator.mediaDevices.getUserMedia({ video: true });
            const videoTrack = newStream.getVideoTracks()[0];
            
            if (localStream) {
                localStream.addTrack(videoTrack);
            } else {
                localStream = await navigator.mediaDevices.getUserMedia({ audio: true, video: true });
            }
            
            // Troca proativamente o Track com os Peers na mesma sala
            Object.values(activeCalls).forEach(call => {
                if (call.peerConnection) {
                    const sender = call.peerConnection.getSenders().find(s => s.track && s.track.kind === 'video');
                    if (sender) {
                        sender.replaceTrack(videoTrack).catch(e => console.error(e));
                    } else if (call.peerConnection.signalingState !== 'closed') {
                        call.peerConnection.addTrack(videoTrack, localStream);
                    }
                }
            });

            const localVid = document.getElementById('localVideo');
            if (localVid) localVid.srcObject = localStream;
        }

        updateControlsUI();
        updateLocalVideo();
        broadcastStateChange();
    } catch (e) {
        console.error('[WebRTC] Camera erro:', e);
        showToast('Erro ao alternar câmera', 'error');
        isCameraOn = false; window.isCameraOn = false;
        updateControlsUI();
        updateLocalVideo();
    } finally {
        if (btnCam) btnCam.classList.remove('loading');
        if (window.performSync) window.performSync();
    }
}

// ══════════════════════════════════════════════
//  Device Selection (Microphone Dropdown)
// ══════════════════════════════════════════════
async function populateMicrophones() {
    const menuList = document.getElementById('micDeviceList');
    if (!menuList) return;

    try {
        const devices = await navigator.mediaDevices.enumerateDevices();
        const audioInputs = devices.filter(device => device.kind === 'audioinput');

        menuList.innerHTML = '';
        audioInputs.forEach((device, index) => {
            const label = device.label || `Microfone ${index + 1}`;
            const div = document.createElement('div');
            div.className = 'device-item';
            div.textContent = label;
            div.onclick = () => switchMicrophone(device.deviceId, label);
            menuList.appendChild(div);
        });

        if (audioInputs.length === 0) {
            menuList.innerHTML = '<div class="device-item" style="color:var(--text-muted);cursor:default;">Nenhum microfone encontrado</div>';
        }
    } catch(e) {
         menuList.innerHTML = '<div class="device-item" style="color:var(--text-muted);cursor:default;">Erro ao ler microfones API</div>';
    }
}

async function switchMicrophone(deviceId, label) {
    document.getElementById('micDeviceMenu').classList.add('hidden');
    try {
        if (localStream) {
            const audioStream = await navigator.mediaDevices.getUserMedia({
                audio: { deviceId: { exact: deviceId } }
            });
            const newAudioTrack = audioStream.getAudioTracks()[0];
            
            localStream.getAudioTracks().forEach(t => { t.stop(); localStream.removeTrack(t); });
            localStream.addTrack(newAudioTrack);
            if (isMuted) newAudioTrack.enabled = false;

            Object.values(activeCalls).forEach(call => {
                if (call.peerConnection) {
                    const sender = call.peerConnection.getSenders().find(s => s.track && s.track.kind === 'audio');
                    if (sender) sender.replaceTrack(newAudioTrack).catch(e=>console.warn(e));
                }
            });
        }
        showToast(`Microfone Modificado para: ${label}`, 'success');
    } catch (e) {
        console.error('[WebRTC] Switch Mic Error:', e);
        showToast('Erro ao trocar de microfone nativo', 'error');
    }
}

// ══════════════════════════════════════════════
//  Incoming Call Alert (Native HTML TocaChat)
// ══════════════════════════════════════════════
function playRingtone() {
    try {
        if (!ringtoneCtx) {
            const AudioContext = window.AudioContext || window.webkitAudioContext;
            ringtoneCtx = new AudioContext();
        }
        if (ringtoneCtx.state === 'suspended') {
            ringtoneCtx.resume();
        }
        const osc = ringtoneCtx.createOscillator();
        const gain = ringtoneCtx.createGain();
        osc.connect(gain);
        gain.connect(ringtoneCtx.destination);
        osc.type = 'sine';

        let time = ringtoneCtx.currentTime;
        for (let i = 0; i < 10; i++) {
            osc.frequency.setValueAtTime(440, time);
            osc.frequency.setValueAtTime(480, time + 0.1);
            gain.gain.setValueAtTime(0, time);
            gain.gain.linearRampToValueAtTime(0.5, time + 0.1);
            gain.gain.setValueAtTime(0.5, time + 1.0);
            gain.gain.linearRampToValueAtTime(0, time + 1.1);
            time += 5.0;
        }

        osc.start();
        ringtoneInterval = osc;
    } catch (e) { }
}

function stopRingtone() {
    if (ringtoneInterval) {
        try { ringtoneInterval.stop(); } catch (e) { }
        ringtoneInterval = null;
    }
    if (ringtoneCtx) {
        try { ringtoneCtx.close(); } catch (e) { }
        ringtoneCtx = null;
    }
}

function showIncomingCallAlert(callConv, callerMember, dados) {
    if (isInCall) return;
    dismissIncomingCall();

    const overlay = document.createElement('div');
    overlay.id = 'incomingCallOverlay';
    overlay.className = 'incoming-call-overlay';

    const callerPhoto = callerMember.foto
        ? `<img src="${callerMember.foto}" alt="${callerMember.nome}" class="incoming-call-avatar-img">`
        : `<video autoplay loop muted playsinline class="default-avatar-vid" style="width:100%; height:100%; object-fit:cover; border-radius:50%;"><source src="/static/images/Criação_de_Animação_Abstrata_Anime.mp4" type="video/mp4"></video>`;

    const subtitle = callConv.tipo === 'grupo' ? `em ${callConv.nome || 'Grupo'}` : 'Chamada direta';

    overlay.innerHTML = `
        <div class="incoming-call-backdrop" onclick="dismissIncomingCall()"></div>
        <div class="incoming-call-card">
            <div class="incoming-call-ring-effect"></div>
            <div class="incoming-call-avatar">${callerPhoto}</div>
            <div class="incoming-call-info">
                <h3 class="incoming-call-name">${callerMember.nome}</h3>
                <p class="incoming-call-subtitle">${subtitle}</p>
                <p class="incoming-call-type">📞 Chamada de vídeo/voz</p>
            </div>
            <div class="incoming-call-actions">
                <button class="incoming-call-btn incoming-call-accept" id="btnAcceptCall" title="Atender">
                    <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M22 16.92v3a2 2 0 0 1-2.18 2 19.79 19.79 0 0 1-8.63-3.07 19.5 19.5 0 0 1-6-6 19.79 19.79 0 0 1-3.07-8.67A2 2 0 0 1 4.11 2h3a2 2 0 0 1 2 1.72c.127.96.361 1.903.7 2.81a2 2 0 0 1-.45 2.11L8.09 9.91a16 16 0 0 0 6 6l1.27-1.27a2 2 0 0 1 2.11-.45c.907.339 1.85.573 2.81.7A2 2 0 0 1 22 16.92z"/></svg>
                </button>
                <button class="incoming-call-btn incoming-call-decline" id="btnDeclineCall" title="Recusar">
                    <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M10.68 13.31a16 16 0 0 0 3.41 2.6l1.27-1.27a2 2 0 0 1 2.11-.45c.907.339 1.85.573 2.81.7A2 2 0 0 1 22 16.92v3a2 2 0 0 1-2.18 2 19.79 19.79 0 0 1-8.63-3.07 19.5 19.5 0 0 1-6-6 19.79 19.79 0 0 1-3.07-8.67A2 2 0 0 1 4.11 2h3a2 2 0 0 1 2 1.72c.127.96.361 1.903.7 2.81a2 2 0 0 1-.45 2.11L8.09 9.91"/><line x1="1" y1="1" x2="23" y2="23"/></svg>
                </button>
            </div>
        </div>
    `;

    document.body.appendChild(overlay);
    playRingtone();
    requestAnimationFrame(() => overlay.classList.add('visible'));

    document.getElementById('btnAcceptCall').onclick = async () => {
        dismissIncomingCall();
        window.callSourceId = callConv.id;
        if (!window.conversaAtual || window.conversaAtual.id !== callConv.id) {
            if (typeof abrirConversa === 'function') await abrirConversa(callConv.id);
        }
        await joinCall();
    };

    document.getElementById('btnDeclineCall').onclick = () => {
        dismissIncomingCall();
        sendCallSignal(callerMember.id, 'decline', {}, callConv.id);
    };

    incomingCallTimeout = setTimeout(dismissIncomingCall, 30000);
}

function dismissIncomingCall() {
    stopRingtone();
    if (incomingCallTimeout) {
        clearTimeout(incomingCallTimeout);
        incomingCallTimeout = null;
    }
    const overlay = document.getElementById('incomingCallOverlay');
    if (overlay) {
        overlay.classList.remove('visible');
        setTimeout(() => overlay.remove(), 300);
    }
}

// ══════════════════════════════════════════════
//  Binds
// ══════════════════════════════════════════════
window.addEventListener('DOMContentLoaded', () => {
    // Inicia a malha silenciosamente em plano de fundo quando a página carregar
    // Desabilitado, agora conectamos apenas no `joinCall` de primeiro uso, economiza recursos e conexões locais.

    const btnAudio = document.getElementById('btnCallAudio');
    if (btnAudio) btnAudio.addEventListener('click', () => {
        if (!isInCall) joinCall();
    });

    const btnVideo = document.getElementById('btnCallVideo');
    if (btnVideo) btnVideo.addEventListener('click', () => {
        if (!isInCall) {
            isCameraOn = true; window.isCameraOn = true;
            joinCall();
        } else {
            toggleCamera();
        }
    });

    const btnMute = document.getElementById('btnMuteMic');
    if (btnMute) btnMute.addEventListener('click', toggleMute);

    const btnCam = document.getElementById('btnToggleCamera');
    if (btnCam) btnCam.addEventListener('click', toggleCamera);

    const btnEnd = document.getElementById('btnEndCall');
    if (btnEnd) btnEnd.addEventListener('click', () => endCall(true));

    const btnBack = document.getElementById('btnBackToMessages');
    if (btnBack) btnBack.addEventListener('click', hideCallView);

    // Dropdown de Microfone (WebRTCbindings agora)
    const btnMicOpts = document.getElementById('btnMicOptions');
    if (btnMicOpts) {
        btnMicOpts.addEventListener('click', (e) => {
            e.stopPropagation();
            const menu = document.getElementById('micDeviceMenu');
            menu.classList.toggle('hidden');
        });

        document.addEventListener('click', (e) => {
            const menu = document.getElementById('micDeviceMenu');
            if (menu && !menu.classList.contains('hidden') && !e.target.closest('.mic-control-group')) {
                menu.classList.add('hidden');
            }
        });
    }
});