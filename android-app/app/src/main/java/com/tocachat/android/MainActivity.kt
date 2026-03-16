package com.tocachat.android

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import com.tocachat.android.data.repository.ChatRepository
import com.tocachat.android.ui.chat.ChatScreen
import com.tocachat.android.ui.conversations.ConversationsScreen
import com.tocachat.android.ui.theme.TocaChatTheme

class MainActivity : ComponentActivity() {
    private val chatRepository = ChatRepository()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            TocaChatTheme {
                TocaChatApp(chatRepository = chatRepository)
            }
        }
    }
}

@Composable
fun TocaChatApp(
    modifier: Modifier = Modifier,
    chatRepository: ChatRepository
) {
    val navController = rememberNavController()

    NavHost(
        navController = navController,
        startDestination = "conversations",
        modifier = modifier
    ) {
        composable("conversations") {
            ConversationsScreen(
                conversations = chatRepository.getConversations(),
                onConversationClick = { conversationId ->
                    navController.navigate("chat/$conversationId")
                }
            )
        }
        composable("chat/{conversationId}") { backStackEntry ->
            val conversationId = backStackEntry.arguments?.getString("conversationId") ?: return@composable
            ChatScreen(
                conversation = chatRepository.getConversation(conversationId),
                onSend = { content ->
                    chatRepository.sendMessage(conversationId = conversationId, content = content)
                }
            )
        }
    }
}
