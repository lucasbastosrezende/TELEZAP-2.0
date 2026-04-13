#include "ui_components.h"

namespace tocachat {

namespace {

float EstimateMessageHeight(const Mensagem& mensagem) {
    const float base = 40.0f;
    const float extra_lines = std::ceil(static_cast<float>(mensagem.conteudo.size()) / 38.0f);
    return base + std::max(1.0f, extra_lines) * 20.0f;
}

LoginUiLayout BuildLoginLayout(const AppState& state) {
    LoginUiLayout layout{};
    layout.root = D2D1::RectF(0.0f, 0.0f, state.window_width(), state.window_height());

    const float card_width = ClampFloat(state.window_width() * 0.36f, 360.0f, 440.0f);
    const float card_height = 390.0f;
    const float left = (state.window_width() - card_width) * 0.5f;
    const float top = (state.window_height() - card_height) * 0.5f;
    layout.card = D2D1::RectF(left, top, left + card_width, top + card_height);

    const FlexOptions options{
        LayoutDirection::Column,
        AlignItems::Stretch,
        JustifyContent::Start,
        14.0f,
        EdgeInsets{28.0f, 28.0f, 28.0f, 28.0f}
    };

    const std::vector<FlexItem> items = {
        FlexItem{86.0f, 0.0f, -1.0f},
        FlexItem{54.0f, 0.0f, -1.0f},
        FlexItem{54.0f, 0.0f, -1.0f},
        FlexItem{48.0f, 0.0f, -1.0f},
        FlexItem{24.0f, 0.0f, -1.0f},
        FlexItem{52.0f, 0.0f, -1.0f}
    };

    const std::vector<D2D1_RECT_F> rects = LayoutEngine::Flex(layout.card, options, items);
    if (rects.size() == 6) {
        layout.header_block = rects[0];
        layout.username_rect = rects[1];
        layout.password_rect = rects[2];
        layout.primary_button_rect = rects[3];
        layout.secondary_button_rect = rects[4];
        layout.hint_rect = rects[5];
    }

    return layout;
}

ChatUiLayout BuildChatLayout(const AppState& state) {
    ChatUiLayout layout{};
    layout.root = D2D1::RectF(0.0f, 0.0f, state.window_width(), state.window_height());

    const FlexOptions root_options{
        LayoutDirection::Row,
        AlignItems::Stretch,
        JustifyContent::Start,
        16.0f,
        EdgeInsets{18.0f, 18.0f, 18.0f, 18.0f}
    };

    const float sidebar_basis = ClampFloat(state.window_width() * 0.26f, 280.0f, 340.0f);
    const std::vector<FlexItem> root_items = {
        FlexItem{sidebar_basis, 0.0f, -1.0f},
        FlexItem{0.0f, 1.0f, -1.0f}
    };

    const std::vector<D2D1_RECT_F> columns = LayoutEngine::Flex(layout.root, root_options, root_items);
    if (columns.size() == 2) {
        layout.sidebar = columns[0];
        layout.main_panel = columns[1];
    }

    const FlexOptions sidebar_options{
        LayoutDirection::Column,
        AlignItems::Stretch,
        JustifyContent::Start,
        12.0f,
        EdgeInsets{16.0f, 16.0f, 16.0f, 16.0f}
    };
    const std::vector<FlexItem> sidebar_items = {
        FlexItem{60.0f, 0.0f, -1.0f},
        FlexItem{0.0f, 1.0f, -1.0f}
    };
    const std::vector<D2D1_RECT_F> sidebar_rects = LayoutEngine::Flex(layout.sidebar, sidebar_options, sidebar_items);
    if (sidebar_rects.size() == 2) {
        layout.sidebar_header = sidebar_rects[0];
        layout.conversation_list = sidebar_rects[1];
    }

    layout.logout_button = D2D1::RectF(
        layout.sidebar_header.right - 82.0f,
        layout.sidebar_header.top + 10.0f,
        layout.sidebar_header.right,
        layout.sidebar_header.top + 40.0f);

    const FlexOptions main_options{
        LayoutDirection::Column,
        AlignItems::Stretch,
        JustifyContent::Start,
        12.0f,
        EdgeInsets{18.0f, 18.0f, 18.0f, 18.0f}
    };
    const std::vector<FlexItem> main_items = {
        FlexItem{64.0f, 0.0f, -1.0f},
        FlexItem{0.0f, 1.0f, -1.0f},
        FlexItem{72.0f, 0.0f, -1.0f}
    };
    const std::vector<D2D1_RECT_F> main_rects = LayoutEngine::Flex(layout.main_panel, main_options, main_items);
    if (main_rects.size() == 3) {
        layout.chat_header = main_rects[0];
        layout.message_list = main_rects[1];
        layout.composer = main_rects[2];
    }

    const FlexOptions composer_options{
        LayoutDirection::Row,
        AlignItems::Center,
        JustifyContent::Start,
        12.0f,
        EdgeInsets{12.0f, 12.0f, 12.0f, 12.0f}
    };
    const std::vector<FlexItem> composer_items = {
        FlexItem{0.0f, 1.0f, 48.0f},
        FlexItem{54.0f, 0.0f, 48.0f}
    };
    const std::vector<D2D1_RECT_F> composer_rects = LayoutEngine::Flex(layout.composer, composer_options, composer_items);
    if (composer_rects.size() == 2) {
        layout.composer_input = composer_rects[0];
        layout.send_button = composer_rects[1];
    }

    const float row_height = 72.0f;
    float row_top = layout.conversation_list.top;
    for (size_t index = 0; index < state.conversations().size(); ++index) {
        const float next_bottom = row_top + row_height;
        if (next_bottom > layout.conversation_list.bottom) {
            break;
        }

        ConversationRowLayout row{};
        row.index = index;
        row.selected = index == state.selected_conversation_index();
        row.rect = D2D1::RectF(layout.conversation_list.left, row_top, layout.conversation_list.right, next_bottom);
        layout.conversation_rows.push_back(row);
        row_top = next_bottom + 8.0f;
    }

    const std::vector<Mensagem>& messages = state.selected_messages();
    float bubble_top = layout.message_list.top + 8.0f;
    const float max_bubble_width = WidthOf(layout.message_list) * 0.62f;

    const Usuario* current_user = state.current_user();
    for (size_t index = 0; index < messages.size(); ++index) {
        const Mensagem& mensagem = messages[index];
        const bool own_message = current_user != nullptr && mensagem.remetente_id == current_user->id;
        const float bubble_height = EstimateMessageHeight(mensagem);
        const float bubble_width = std::min(max_bubble_width, 250.0f + static_cast<float>(mensagem.conteudo.size()) * 1.2f);
        const float bubble_left = own_message
            ? layout.message_list.right - bubble_width - 12.0f
            : layout.message_list.left + 12.0f;

        MessageBubbleLayout bubble{};
        bubble.index = index;
        bubble.own_message = own_message;
        bubble.rect = D2D1::RectF(bubble_left, bubble_top, bubble_left + bubble_width, bubble_top + bubble_height);
        layout.message_rows.push_back(bubble);
        bubble_top += bubble_height + 12.0f;
    }

    return layout;
}

}  // namespace

ShellUiLayout UiComponents::Build(const AppState& state) {
    ShellUiLayout layout{};
    layout.login = BuildLoginLayout(state);
    layout.chat = BuildChatLayout(state);
    return layout;
}

const ConversationRowLayout* UiComponents::HitTestConversation(const ChatUiLayout& layout, float x, float y) noexcept {
    for (const ConversationRowLayout& row : layout.conversation_rows) {
        if (PointInRect(row.rect, x, y)) {
            return &row;
        }
    }

    return nullptr;
}

}  // namespace tocachat
