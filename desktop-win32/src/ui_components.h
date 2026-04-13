#pragma once

#include "app_state.h"
#include "layout_engine.h"

namespace tocachat {

enum class AuthFocusField {
    None,
    Username,
    Password
};

struct AuthViewState {
    std::wstring username_text;
    std::wstring password_text;
    AuthFocusField focus = AuthFocusField::Username;
    std::wstring helper_text;
};

struct ConversationRowLayout {
    D2D1_RECT_F rect{};
    size_t index = 0;
    bool selected = false;
};

struct MessageBubbleLayout {
    D2D1_RECT_F rect{};
    size_t index = 0;
    bool own_message = false;
};

struct LoginUiLayout {
    D2D1_RECT_F root{};
    D2D1_RECT_F card{};
    D2D1_RECT_F header_block{};
    D2D1_RECT_F username_rect{};
    D2D1_RECT_F password_rect{};
    D2D1_RECT_F primary_button_rect{};
    D2D1_RECT_F secondary_button_rect{};
    D2D1_RECT_F hint_rect{};
};

struct ChatUiLayout {
    D2D1_RECT_F root{};
    D2D1_RECT_F sidebar{};
    D2D1_RECT_F sidebar_header{};
    D2D1_RECT_F logout_button{};
    D2D1_RECT_F conversation_list{};
    D2D1_RECT_F main_panel{};
    D2D1_RECT_F chat_header{};
    D2D1_RECT_F message_list{};
    D2D1_RECT_F composer{};
    D2D1_RECT_F composer_input{};
    D2D1_RECT_F send_button{};
    std::vector<ConversationRowLayout> conversation_rows;
    std::vector<MessageBubbleLayout> message_rows;
};

struct ShellUiLayout {
    LoginUiLayout login;
    ChatUiLayout chat;
};

class UiComponents {
public:
    static ShellUiLayout Build(const AppState& state);
    static const ConversationRowLayout* HitTestConversation(const ChatUiLayout& layout, float x, float y) noexcept;
};

}  // namespace tocachat
