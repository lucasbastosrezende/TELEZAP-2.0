const express = require('express');
const proxy = require('express-http-proxy');
const app = express();

const PORT = 8080;
const FLASK_SERVER = 'http://localhost:3000';
const PEERJS_SERVER = 'http://localhost:9000';

console.log('--- Gateway TocaChat ---');

// Roteamento para o PeerJS (myapp)
app.use('/myapp', proxy(PEERJS_SERVER, {
    proxyReqPathResolver: (req) => {
        return '/myapp' + req.url;
    },
    userResHeaderDecorator(headers, userReq, userRes, proxyReq, proxyRes) {
        // Garante que headers de WebSocket e CORS passem corretamente
        return headers;
    }
}));

// Todo o resto vai para o Flask
app.use('/', proxy(FLASK_SERVER));

app.listen(PORT, () => {
    console.log(`Gateway unificado rodando em http://localhost:${PORT}`);
    console.log(`- Redirecionando /myapp para PeerJS (9000)`);
    console.log(`- Redirecionando todo o resto para Flask (3000)`);
    console.log('\nAgora você pode rodar: cloudflared tunnel --url http://localhost:8080');
});
