/*
 * socket demonstrations:
 * This is the server side of an "internet domain" socket connection, for
 * communicating over the network.
 *
 * In this case we are willing to wait for chatter from the client
 * _or_ for a new connection.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#ifndef PORT
    #define PORT 51360
#endif

# define SECONDS 10

struct client {
    int fd;
    struct in_addr ipaddr;
    char name[50];
    struct client *next;
    struct client *opponent;     // Current opponent in an ongoing match
    struct client *last_opponent; // Most recent opponent after a match ends
    int in_game;
    int hitpoints;
    int power_moves;
    int is_turn;
    int name_set;
    int speaking;
    char speak_buffer[100];
    int speak_count;
    int mute_toggle;
};
static struct client *addclient(struct client *top, int fd, struct in_addr addr);
static struct client *removeclient(struct client *top, int fd);
//static void broadcast(struct client *top, char *s, int size);
static void broadcast(struct client *top, char *s, int size, int exclude_fd);
int handleclient(struct client *p, struct client *top);
void end_match(struct client **top, struct client *p1, struct client *p2);
static struct client *match_opponent(struct client *top, struct client *current);
void start_battle(struct client *p1, struct client *p2);
void move_client_end(struct client **top, struct client *move);


int bindandlisten(void);

int main(void) {
    srand(time(NULL));

    int clientfd, maxfd, nready;
    struct client *opponent;
    struct client *head = malloc(sizeof(struct client));  // Allocate memory for the dummy head node
    if (head == NULL) {
        perror("malloc");
        exit(1);
    }

    // Initialize the dummy head node
    head->name[0] = 'S';  
    head->name[1] = '\0';
    head->in_game = 1;
    head->name_set = 1;
    head->next = NULL;  

    socklen_t len;
    struct sockaddr_in q;
    struct timeval tv;
    fd_set allset;
    fd_set rset;

    int listenfd = bindandlisten();
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    maxfd = listenfd;

    while (1) {
        rset = allset;
        tv.tv_sec = SECONDS;
        tv.tv_usec = 0;

        nready = select(maxfd + 1, &rset, NULL, NULL, &tv);
        if (nready == 0) {
            printf("No response from clients in %d seconds\n", SECONDS);
            continue;
        }

        if (nready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)) {
            len = sizeof(q);
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }

            // Set the socket to non-blocking mode
            int flags = fcntl(clientfd, F_GETFL, 0);
            if (flags == -1) {
                perror("fcntl");
                close(clientfd); // Handle error appropriately
        } else {
                if (fcntl(clientfd, F_SETFL, flags | O_NONBLOCK) == -1) {
                    perror("fcntl");
                    close(clientfd); // Handle error appropriately
            }
        }

        FD_SET(clientfd, &allset);
        if (clientfd > maxfd) {
            maxfd = clientfd;
        }
        addclient(head, clientfd, q.sin_addr);
    }

        struct client *p = head->next;
        struct client *next = NULL;

        while (p != NULL) {
            next = p->next;

            if (FD_ISSET(p->fd, &rset)) {
                int result = handleclient(p, head);
                if (result == -1) {
                    FD_CLR(p->fd, &allset);
                    close(p->fd);
                    head = removeclient(head, p->fd);
            }
        }

             p = next;
        }

        // Matchmaking loop: find opponents for clients who are not in a game
        for (struct client *p = head->next; p != NULL; p = p->next) {
            if (!p->in_game && p->name_set) {
                opponent = match_opponent(head, p);
                if (opponent) {
                    printf("%s and %s have been matched for a battle.\n", p->name, opponent->name);
                }
            }
        }
    }

    return 0;
}

int handleclient(struct client *p, struct client *top) {
   char buf[100], feedback[700], opponent_feedback[700] = "";
   char broadcast_msg[512];
   int len = read(p->fd, buf, sizeof(buf) - 1);

    if (len > 0) {
        // Remove newline character if present at the end of the input buffer
        //if (buf[len - 1] == '\n') {
            //buf[len - 1] = '\0';
            //len--;
            //if (len > 0 && buf[len - 1] == '\r') {
                // Also remove carriage return character if present (for CR+LF endings)
                //buf[len - 1] = '\0';
            //}
        //}

        buf[len] = '\0';

    if (!p->name_set) {
        char *newline = strchr(buf, '\n');
        if (newline) {
            *newline = '\0';  // Terminate at the newline character
            size_t name_length = newline - buf; // Calculate the length of the input before the newline
            if (name_length > sizeof(p->name) - 1 - strlen(p->name)) {
                name_length = sizeof(p->name) - 1 - strlen(p->name); // Limit to available space
            }
            strncat(p->name, buf, name_length);
            p->name_set = 1;

            snprintf(feedback, sizeof(feedback), "\nWelcome, %s! Awaiting opponent...\r\n", p->name);
            send(p->fd, feedback, strlen(feedback), 0);

            snprintf(broadcast_msg, sizeof(broadcast_msg), "\n*****%s joins the Arena******\r\n", p->name);
            broadcast(top, broadcast_msg, strlen(broadcast_msg), p->fd);

         printf("Adding client %s\n", p->name);
        }else {
            size_t space_left = sizeof(p->name) - strlen(p->name) - 1;
            if (space_left > 0) {
                strncat(p->name, buf, space_left);
            }
        // No return here to allow further processing
    }
}

        if (!p->speaking) {
            p->speaking = 0;
            memset(p->speak_buffer, 0, sizeof(p->speak_buffer));
        }

        if (!p->in_game || !p->is_turn) {
            if (p->in_game && !p->is_turn){
                if (buf[0] == 'm'){
                   if (p->mute_toggle == 0){
                    p->mute_toggle = 1;
                    snprintf(feedback, sizeof(feedback), "\nChat is now muted.\n\nWaiting for %s to strike...\n",
                    p->opponent->name);
                    send(p->fd, feedback, strlen(feedback), 0);

                   }else{
                    p->mute_toggle = 0;
                    snprintf(feedback, sizeof(feedback), "\nChat is now unmuted.\n\nWaiting for %s to strike...\n",
                    p->opponent->name);
                    send(p->fd, feedback, strlen(feedback), 0);
                   }

            }}

            return 0;
        }
        if (p->in_game && p->is_turn && p->name_set){
            if (p->power_moves > 0){
                if (p->speaking) {
                    size_t buffer_length = strlen(p->speak_buffer);
                    size_t max_append = sizeof(p->speak_buffer) - buffer_length - 1; // -1 for null terminator

                    char *newline = strchr(buf, '\n');
                    if (newline) {
                        // If newline is found, copy only up to the newline
                        size_t copy_length = newline - buf;
                        if (copy_length > max_append) {
                            copy_length = max_append;
                        }
                        strncat(p->speak_buffer, buf, copy_length);
                        p->speak_buffer[buffer_length + copy_length] = '\0'; // Ensure null-termination
                        p->speaking = 0;
                        snprintf(feedback, sizeof(feedback),
                         "\nYou speak: %s\n\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(p)owermove\n(s)peak something\n(m)ute chat\n",
                         p->speak_buffer, p->hitpoints,p->power_moves, p->opponent->name, p->opponent->hitpoints);
                         if (p->opponent->power_moves > 0){
                            snprintf(opponent_feedback, sizeof(opponent_feedback),
                             "\n\n%s takes a break to tell you:\n%s\n\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\nWaiting for %s to strike...\n",
                            p->name, p->speak_buffer, p->opponent->hitpoints, p->opponent->power_moves, p->name, p->hitpoints, p->name);
                         }else if (p->opponent->power_moves == 0){
                            snprintf(opponent_feedback, sizeof(opponent_feedback),
                             "\n\n%s takes a break to tell you:\n%s\n\nYour hitpoints: %d\n\n%s's hitpoints: %d\nWaiting for %s to strike...\n",
                            p->name, p->speak_buffer, p->opponent->hitpoints, p->name, p->hitpoints, p->name);
                         }
                        
                        memset(p->speak_buffer, 0, sizeof(p->speak_buffer)); // Clear buffer
                    } else {
                    // Add character to buffer
                        if (max_append > 0) {
                            size_t chunk_length = (size_t)len < max_append ? (size_t)len : max_append;
                            strncat(p->speak_buffer, buf, chunk_length);
                            p->speak_buffer[buffer_length + chunk_length] = '\0'; // Ensure null-termination
                }
                        return 0;
                     }
                }else if (buf[0] == 'm'){
                   if (p->mute_toggle == 0){
                    p->mute_toggle = 1;
                    snprintf(feedback, sizeof(feedback),
                     "\nChat is now muted.\n\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(p)owermove\n(s)peak something\n(m)ute chat\n",
                     p->hitpoints, p->power_moves, p->opponent->name, p->opponent->hitpoints);

                   }else{
                    p->mute_toggle = 0;
                    snprintf(feedback, sizeof(feedback),
                     "\nChat is now unmuted.\n\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(p)owermove\n(s)peak something\n(m)ute chat\n",
                     p->hitpoints, p->power_moves, p->opponent->name, p->opponent->hitpoints);
                   }
                }else if (buf[0] == 'a') {
                    p->opponent->hitpoints = p->opponent->hitpoints > 5 ? p->opponent->hitpoints - 5 : 0;
                    sprintf(feedback,
                     "\nYou hit %s for 5 damage!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\nWaiting for %s to strike...\n",
                    p->opponent->name, p->hitpoints, p->power_moves, p->opponent->name, p->opponent->hitpoints, p->opponent->name);
                    if (p->opponent->power_moves > 0){
                        sprintf(opponent_feedback,
                         "\n%s hits you for 5 damage!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(p)owermove\n(s)peak something\n(m)ute chat\n",
                        p->name, p->opponent->hitpoints, p->opponent->power_moves, p->name, p->hitpoints);
                    }else if(p->opponent->power_moves == 0){
                        sprintf(opponent_feedback,
                         "\n%s hits you for 5 damage!\nYour hitpoints: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(s)peak something\n(m)ute chat\n",
                        p->name, p->opponent->hitpoints, p->name, p->hitpoints);
                    }
                    p->is_turn = 0;
                    p->opponent->is_turn = 1;
                    p->speak_count = 0;
                } else if (buf[0] == 'p') {
                    int hit = rand() % 100 < 40;
                    int damage = hit ? 15 : 0;
                    p->opponent->hitpoints = p->opponent->hitpoints > damage ? p->opponent->hitpoints - damage : 0;
                    p->power_moves--;
                    if (hit){
                        sprintf(feedback,
                        "\nYou hit %s for 15 damage!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\nWaiting for %s to strike...\n",
                     p->opponent->name,  p->hitpoints, p->power_moves, p->opponent->name, p->opponent->hitpoints, p->opponent->name);
                        if (p->opponent->power_moves > 0){
                            sprintf(opponent_feedback,
                             "\n%s powermoves you for 15 damage!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(p)owermove\n(s)peak something\n(m)ute chat\n",
                         p->name, p->opponent->hitpoints, p->opponent->power_moves, p->name, p->hitpoints);
                        }else if (p->opponent->power_moves == 0){
                            sprintf(opponent_feedback,
                             "\n%s powermoves you for 15 damage!\nYour hitpoints: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(s)peak something\n(m)ute chat\n",
                         p->name, p->opponent->hitpoints, p->name, p->hitpoints);
                        }
                    }else if (!hit){
                        sprintf(feedback,
                         "\nYou missed!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\nWaiting for %s to strike...\n",
                        p->hitpoints, p->power_moves, p->opponent->name, p->opponent->hitpoints, p->opponent->name);
                         if (p->opponent->power_moves > 0){
                            sprintf(opponent_feedback,
                             "\n%s's powermove missed!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(p)owermove\n(s)peak something\n(m)ute chat\n",
                         p->name, p->opponent->hitpoints, p->opponent->power_moves, p->name, p->hitpoints);
                        }else if (p->opponent->power_moves == 0){
                            sprintf(opponent_feedback,
                             "\n%s's powermove missed!\nYour hitpoints: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(s)peak something\n(m)ute chat\n",
                         p->name, p->opponent->hitpoints, p->name, p->hitpoints);
                        }
                    }
                    p->is_turn = 0;
                    p->opponent->is_turn = 1;
                    p->speak_count = 0;
                
                }else if (buf[0] == 's' && !p->speaking){
                    // Enter speaking mode
                    if (p->opponent->mute_toggle == 1){
                        snprintf(feedback, sizeof(feedback),
                         "\nYou cannot speak with %s, their mute toggle is on.\n\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints:%d\n\nIt's your turn:\n(a)ttack\n(p)owermove\n(s)peak something\n(m)ute chat\n",
                        p->opponent->name, p->hitpoints, p->power_moves, p->opponent->name, p->opponent->hitpoints);
                    }else if (p->speak_count == 3){
                       snprintf(feedback, sizeof(feedback),
                        "\nYou have spoken enough! It is time to attack.\n\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints:%d\n\nIt's your turn:\n(a)ttack\n(p)owermove\n(m)ute chat\n",
                        p->hitpoints, p->power_moves, p->opponent->name, p->opponent->hitpoints);
                        p->speak_count++;
                    }else if (p->speak_count == 4){
                        return 0;
                }else{
                    p->speaking = 1;
                    p->speak_count++;
                    snprintf(feedback, sizeof(feedback), "\nSpeak: ");
                }
                }else{
                    return 0;
                }
           }else if (p->power_moves == 0){
                if (p->speaking) {
                    // In speaking mode, collect characters into buffer
                    size_t buffer_length = strlen(p->speak_buffer);
                    size_t max_append = sizeof(p->speak_buffer) - buffer_length - 1; // -1 for null terminator

                    char *newline = strchr(buf, '\n');
                    if (newline) {
                        // If newline is found, copy only up to the newline
                        size_t copy_length = newline - buf;
                        if (copy_length > max_append) {
                            copy_length = max_append;
                        }
                        strncat(p->speak_buffer, buf, copy_length);
                        p->speak_buffer[buffer_length + copy_length] = '\0'; // Ensure null-termination
                        p->speaking = 0;
                        snprintf(feedback, sizeof(feedback),
                         "You speak: %s\nYour hitpoints: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(s)peak something\n(m)ute chat\n",
                         p->speak_buffer, p->hitpoints, p->opponent->name,  p->opponent->hitpoints);
                        if (p->opponent->power_moves > 0){
                            snprintf(opponent_feedback, sizeof(opponent_feedback),
                             "\n\n%s takes a break to tell you:\n%s\n\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\nWaiting for %s to strike...\n",
                            p->name, p->speak_buffer,p->opponent->hitpoints, p->opponent->power_moves, p->name, p->hitpoints, p->name);
                        }else if (p->opponent->power_moves == 0){
                            snprintf(opponent_feedback, sizeof(opponent_feedback),
                             "\n\n%s takes a break to tell you:\n%s\n\nYour hitpoints: %d\n\n%s's hitpoints: %d\nWaiting for %s to strike...\n",
                            p->name, p->speak_buffer,p->opponent->hitpoints, p->name, p->hitpoints, p->name);
                        }
                        memset(p->speak_buffer, 0, sizeof(p->speak_buffer)); // Clear buffer
                    } else {
                    // Add character to buffer
                        if (max_append > 0) {
                            size_t chunk_length = (size_t)len < max_append ? (size_t)len : max_append;
                            strncat(p->speak_buffer, buf, chunk_length);
                            p->speak_buffer[buffer_length + chunk_length] = '\0'; // Ensure null-termination
                        }
                         return 0; // Return early to keep collecting characters
                     }
                }else if (buf[0] == 'm'){
                   if (p->mute_toggle == 0){
                    p->mute_toggle = 1;
                    snprintf(feedback, sizeof(feedback),
                     "\nChat is now muted.\n\nYour hitpoints: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(s)peak something\n(m)ute chat\n",
                     p->hitpoints, p->opponent->name, p->opponent->hitpoints);
                   }else{
                    p->mute_toggle = 0;
                    snprintf(feedback, sizeof(feedback),
                     "\nChat is now unmuted.\n\nYour hitpoints: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(s)peak something\n(m)ute chat\n",
                     p->hitpoints, p->opponent->name, p->opponent->hitpoints);
                   }
                }else if (buf[0] == 'a') {
                    p->opponent->hitpoints = p->opponent->hitpoints > 5 ? p->opponent->hitpoints - 5 : 0;
                     sprintf(feedback,
                      "\nYou hit %s for 5 damage!\nYour hitpoints: %d\n\n%s's hitpoints: %d\nWaiting for %s to strike...\n",
                        p->opponent->name, p->hitpoints, p->opponent->name, p->opponent->hitpoints, p->opponent->name);
                    if (p->opponent->power_moves > 0){
                        sprintf(opponent_feedback,
                         "\n%s hits you for 5 damage!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(p)owermove\n(s)peak something\n(m)ute chat\n",
                        p->name, p->opponent->hitpoints, p->opponent->power_moves, p->name, p->hitpoints);
                    }else if (p->opponent->power_moves == 0){
                        sprintf(opponent_feedback,
                         "\n%s hits you for 5 damage!\nYour hitpoints: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(s)peak something\n(m)ute chat\n",
                        p->name, p->opponent->hitpoints, p->name, p->hitpoints);
                    }
                    p->is_turn = 0;
                    p->opponent->is_turn = 1;
                    p->speak_count = 0;
            }else if (buf[0] == 's' && !p->speaking){
                if (p->opponent->mute_toggle == 1){
                        snprintf(feedback, sizeof(feedback),
                         "\nYou cannot speak with %s, their mute toggle is on.\n\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints:%d\n\nIt's your turn:\n(a)ttack\n(p)owermove\n(s)peak something\n(m)ute chat\n",
                        p->opponent->name, p->hitpoints, p->power_moves, p->opponent->name, p->opponent->hitpoints);
                }else if (p->speak_count == 3){
                        snprintf(feedback, sizeof(feedback),
                         "\nYou have spoken enough! It is time to attack.\n\nYour hitpoints: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(m)ute chat\n",
                         p->hitpoints, p->opponent->name, p->opponent->hitpoints);
                         p->speak_count++;
                    }else if (p->speak_count == 4){
                        return 0;
                    }else{
                        p->speaking = 1;
                        p->speak_count++;
                        snprintf(feedback, sizeof(feedback), "\nSpeak: ");
                }}else{
                    return 0;
                }
                }else{
                    return 0;
                }
        send(p->fd, feedback, strlen(feedback), 0);
        if (opponent_feedback[0] != '\0') {
            send(p->opponent->fd, opponent_feedback, strlen(opponent_feedback), 0);
        }

        if (p->opponent->hitpoints <= 0) {
        sprintf(feedback, "Victory! %s's hitpoints are now 0. You win!\nAwaiting next opponent...\r\n", p->opponent->name);
        sprintf(opponent_feedback, "Defeat! Your hitpoints are now 0. %s wins!\nAwaiting next opponent...\r\n", p->name);
        send(p->fd, feedback, strlen(feedback), 0);
        send(p->opponent->fd, opponent_feedback, strlen(opponent_feedback), 0);
        end_match(&top, p, p->opponent);  
    }

        return 0;
        }
    }else if (len == 0) {
    // Client disconnection
   if (p->in_game && p->opponent != NULL) {
        // Client was in a game, declare opponent as winner
        struct client *opponent = p->opponent;
        char win_msg[256];
        snprintf(win_msg, sizeof(win_msg), "Opponent %s disconnected. You win!\nAwaiting next opponent...\r\n", p->name);
        send(opponent->fd, win_msg, strlen(win_msg), 0);

        opponent->in_game = 0;
        opponent->opponent = NULL;
        opponent->hitpoints = 30;
        opponent->last_opponent = NULL;
        opponent->power_moves = rand() % 3 + 1;    
        opponent->is_turn = 0;                       // Clear the opponent since the match is over
        move_client_end(&top, opponent);
        snprintf(broadcast_msg, sizeof(broadcast_msg), "\n*****%s left the Arena******\r\n", p->name);
        broadcast(top, broadcast_msg, strlen(broadcast_msg), p->fd);
         // Move the winning client to the end of the list
    }else{
        snprintf(broadcast_msg, sizeof(broadcast_msg), "\n*****%s left the Arena******\r\n", p->name);
        broadcast(top, broadcast_msg, strlen(broadcast_msg), p->fd);
    }
    return -1; 
}else{
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
            
            return -1;
        }
}
    return 0;
}

 /* bind and listen, abort on error
  * returns FD of listening socket
  */
int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 10)) {
        perror("listen");
        exit(1);
    }
    return listenfd;
}

static struct client *addclient(struct client *top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    // Initialize the new client's attributes
    p->fd = fd;
    p->ipaddr = addr;
    p->next = NULL;
    p->in_game = 0; // Initially not in a game
    p->hitpoints = 30; 
    p->power_moves = rand() % 3 + 1; 
    p->is_turn = 0; // It's not their turn yet
    p->name[0] = '\0'; // Set the name to an empty string
    p->opponent = NULL; // No opponent yet
    p->last_opponent = NULL; // No last opponent yet
    p->name_set = 0;
    p->speaking = 0;
    p->speak_buffer[0] = '\0';
    p->speak_count = 0;
    p->mute_toggle = 0;
    // Send a welcome message to the client asking for their name
    char *welcome_msg = "Welcome! Please enter your name:";
    if (send(fd, welcome_msg, strlen(welcome_msg), 0) == -1) {
        perror("send");
        free(p); // Cleanup allocation since send failed
        return top;
    }


    if (top == NULL){
        return p;
    }else{
        //Traversing list until the end
        struct client *current = top;
        while(current->next != NULL){
            current = current->next;
        }
        //Append new client
        current->next = p;
    }


    return p; 
}

static struct client *removeclient(struct client *top, int fd) {
    struct client *prev = NULL;
    struct client *cur = top;

    while (cur != NULL && cur->fd != fd) {
        prev = cur;
        cur = cur->next;
    }

    if (cur == NULL) {
        // Client not found
        return top;
    }

    if (prev == NULL) {
        // Removing the first client in the list
        top = cur->next;
    } else {
        // Removing a client that is not the first
        prev->next = cur->next;
    }
    printf("Removing client %s\n", cur->name);
    free(cur);
    return top;
}


static void broadcast(struct client *top, char *s, int size, int exclude_fd) {
    struct client *p;
    // Skip the dummy head node by starting with top->next
    for (p = top->next; p; p = p->next) {
        if (p->fd != exclude_fd) {
            // Send the message to this client
            if (send(p->fd, s, size, 0) < 0) {
                perror("broadcast write");
                // Handle error, e.g., remove the client
            }
        }
    }
}
static struct client *match_opponent(struct client *top, struct client *current) {
    struct client *p, *matched = NULL;

    for (p = top; p != NULL; p = p->next) {
        if (p != current && !p->in_game && p->last_opponent != current && p->name_set) {
            matched = p;
            break;
        }
    }

    if (matched) {
        start_battle(current, matched);
    }

    return matched;
}
void end_match(struct client **top, struct client *p1, struct client *p2) {
    if (!p1 || !p2) {
        return; // Check if either pointer is NULL
    }

    p1->in_game = 0;
    p2->in_game = 0;
    p1->hitpoints = 30;
    p2->hitpoints = 30;
    p1->is_turn = 0;
    p2->is_turn = 0;
    p1->power_moves = rand() % 3 + 1;
    p2->power_moves = rand() % 3 + 1;
    p1->speak_count = 0;
    p2->speak_count = 0;

    // Update last_opponent for future matchmaking logic
    p1->last_opponent = p2;
    p2->last_opponent = p1;

    // Clear current opponent since the match has ended
    p1->opponent = NULL;
    p2->opponent = NULL;

    printf("Match between %s and %s has ended.\n", p1->name, p2->name);

    // Move the clients to the end of the list
    move_client_end(top, p1);
    move_client_end(top, p2);
}
void move_client_end(struct client **top, struct client *move) {
    if (*top == NULL || move == NULL) {
        return;
    }

    if ((*top) == move) {
        return; // If move is the first node, no need to move it.
    }

    struct client *current = *top;
    struct client *prev = NULL;

    while (current != NULL && current != move) {
        prev = current;
        current = current->next;
    }

    // If not found or it's already the last element, no need to move
    if (current == NULL || current->next == NULL) {
        return;
    }

    // Unlink the node from its current position
    if (prev != NULL) {
        prev->next = current->next;
    }

    // Find the last node
    struct client *last = *top;
    while (last->next != NULL) {
        last = last->next;
    }

    // Append the node at the end
    last->next = move;
    move->next = NULL;
}
void start_battle(struct client *p1, struct client *p2) {
    p1->hitpoints = 30;
    p2->hitpoints = 30;
    p1->power_moves = rand() % 3 + 1;
    p2->power_moves = rand() % 3 + 1;
    p1->in_game = 1;
    p2->in_game = 1;
    
    // Set opponents
    p1->opponent = p2;
    p2->opponent = p1;
    p1->speak_count = 0;
    p2->speak_count = 0;
    // Randomly decide who goes first
    if (rand() % 2 == 0) {
        p1->is_turn = 1;
        p2->is_turn = 0;
    } else {
        p1->is_turn = 0;
        p2->is_turn = 1;
    }

    char turn_msg[500];
    char wait_msg[500];

    // Decide who goes first and send the appropriate messages
    if (p1->is_turn) {
        snprintf(turn_msg, sizeof(turn_msg),
         "\nYou engage %s!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(p)owermove\n(s)peak something\n(m)ute chat\n",
         p2->name, p1->hitpoints, p1->power_moves,p2->name, p2->hitpoints);
        snprintf(wait_msg, sizeof(wait_msg),
         "\nYou engage %s!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\nWaiting for %s to strike...\r\n",
         p1->name, p2->hitpoints, p2->power_moves, p1->name, p1->hitpoints, p1->name);
        send(p1->fd, turn_msg, strlen(turn_msg), 0);
        send(p2->fd, wait_msg, strlen(wait_msg), 0);
    } else {
        snprintf(turn_msg, sizeof(turn_msg),
         "\nYou engage %s!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n\nIt's your turn:\n(a)ttack\n(p)owermove\n(s)peak something\n(m)ute chat\n",
         p1->name, p2->hitpoints, p2->power_moves,p1->name, p1->hitpoints);
        snprintf(wait_msg, sizeof(wait_msg),
         "\nYou engage %s!\nYour hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\nWaiting for %s to strike...\r\n",
         p2->name, p1->hitpoints, p1->power_moves, p2->name, p2->hitpoints, p2->name);
        send(p1->fd, wait_msg, strlen(wait_msg), 0);
        send(p2->fd, turn_msg, strlen(turn_msg), 0);
    }

}