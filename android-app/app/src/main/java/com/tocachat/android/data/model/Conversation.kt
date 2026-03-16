package com.tocachat.android.data.model

data class Conversation(
    val id: String,
    val title: String,
    val lastMessagePreview: String,
    val messages: List<Message>
)
