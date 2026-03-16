package com.tocachat.android.data.model

data class Message(
    val id: String,
    val sender: String,
    val content: String,
    val timestamp: String,
    val isMine: Boolean
)
