# 4_in_row

---

### Compilation:

`gcc servidor.c -o server`
`gcc cliente.c -o client`

### Execution:

`./server "port" "number of rows" "number of columns"`
`./client "ip of the server" "port of the server" "username"`

The Connect Four game implemented in C as a client-server system provides an interactive experience between two players connected over a network. In this classic game, two players take turns placing tokens on a vertical board with 7 columns and 6 rows. The objective is to align four tokens of the same color horizontally, vertically, or diagonally before the opponent.

Client-Server Implementation:

The server manages the game logic and communication between the two clients. It uses sockets to establish connections with clients, receive and send players' moves, and verify win conditions. Each client acts as a player in the game, sending their moves to the server and receiving updates on the game state from the server.

Interactive Gameplay:

Players interact with the game through a console interface provided by the client. Each player inputs their move by selecting the column where they wish to drop their token. The server validates moves and updates the board accordingly, ensuring the integrity of the game rules at all times.

Strategy and Enjoyment:

This implementation of Connect Four not only tests players' strategic skills in anticipating opponents' moves but also their ability to collaborate and compete in real-time over the network. The game's simplicity and competitive nature make it appealing to players of all ages and skill levels.

This client-server implementation of Connect Four in C provides a classic gaming experience enhanced by network dynamics, allowing players to compete regardless of their physical location.
