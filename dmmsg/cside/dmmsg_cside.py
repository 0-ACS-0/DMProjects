from dmclient.DMClient import DMClient
import pyray as pr

######################################
# Chat log list and user imput string:
chat_log = []  
user_input = ""  

############
# Callbacks: 
def on_receive(msg: str):
    """ Executed after receiving a message from the server side.
    
    Args:
        - msg(str): String received from the server side. """
    # Global chat log to write to it:
    global chat_log

    # Append the message to the chat log messages:
    chat_log.append(f"[<<]: {msg}")


def on_disconnect():
    """ Executed after detecting a disconnection between client and server sides. """
    # Global chat log to write to it:
    global chat_log

    # Append disconnection message:
    chat_log.append("[sys]: Disconnected from the server.")


def on_connect():
    """ Executed after detecting a disconnection between client and server sides. """
    # Global chat log to write to it:
    global chat_log

    # Append disconnection message:
    chat_log.append("[sys]: Connected to the server.")

#############################
# Client init and connection:
client = DMClient()
client.setOnReceive(on_receive)
client.setOnDisconnect(on_disconnect)
client.setOnConnect(on_connect)
client.connect()

#######################
# Window configuration:
pr.init_window(800, 600, "Chat en línea - Death March")
pr.set_target_fps(60)
font_size = 18

#################
# Custom widgets:
input_rect = pr.Rectangle(10, 550, 780, 30)
button_rect = pr.Rectangle(650, 10, 120, 30)

###############
# Program loop:
while not pr.window_should_close():
    pr.begin_drawing()
    pr.clear_background(pr.GRAY)

    # Show chat log (last 18 messages):
    y_offset = 10
    for msg in chat_log[-18:]:
        pr.draw_text(msg, 10, y_offset, font_size, pr.RAYWHITE)
        y_offset += font_size + 4

    # Connect button:
    mouse_pos = pr.get_mouse_position()
    hovered = pr.check_collision_point_rec(mouse_pos, button_rect)
    button_color = pr.DARKGREEN if hovered else pr.GREEN
    pr.draw_rectangle_rec(button_rect, button_color)
    pr.draw_text("Conectar", int(button_rect.x) + 10, int(button_rect.y) + 8, 20, pr.RAYWHITE)
    if hovered and pr.is_mouse_button_pressed(pr.MOUSE_LEFT_BUTTON):
        if not client.isConnected():
            client.connect()
  

    # Text box:
    pr.draw_rectangle_rec(input_rect, pr.LIGHTGRAY)
    pr.draw_text(user_input + "_", 15, 555, font_size, pr.BLACK)

    # Input capture:
    key = pr.get_char_pressed()
    if key > 0 and 32 <= key <= 126:  # Allowed characters (visible)
        user_input += chr(key)
    
    if pr.is_key_pressed(pr.KEY_BACKSPACE) and user_input:
        user_input = user_input[:-1]

    if pr.is_key_pressed(pr.KEY_ENTER) and client.isConnected():
        if user_input.strip():
            client.send(user_input.strip())
            chat_log.append(f"[Tú]: {user_input.strip()}")
        user_input = ""

    pr.end_drawing()

pr.close_window()
client.disconnect()