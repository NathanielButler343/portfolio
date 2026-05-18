// game.c
#include <stdlib.h>
#include <stdio.h>
#include "game.h"
#include "input.h"
#include "physics.h"
#include "render.h"

Snakes snakes;
Food food;
Walls walls;
PwrUps pwrUps;
GameState state;
InputQueue input_queue;

MenuItem *menu_head;
MenuItem *selected_menu;

MenuItem *start_menu_head;
MenuItem *selected_start_menu;

MenuItem *setting_menu_head;
MenuItem *selected_setting_menu;

MenuItem *multi_player_menu_head;
MenuItem *selected_multi_player_menu;

int settings_opened_from = 0;
int brightness_level = 1; // default normal

// ─────────────────────────────────────────────────────────────────────────────
// SNAKE
// ─────────────────────────────────────────────────────────────────────────────

Segment *create_segment(int x, int y){
    Segment *segment = malloc(sizeof(Segment));
    if (!segment){
        fprintf(stderr, "Error: Failed to allocate memory for segment\n");
        return NULL;
    }
    segment->x = x;
    segment->y = y;
    segment->next = NULL;
    return segment;
}

void snake_add_head(Snake *snake, int x, int y){
    Segment *new_segment = create_segment(x, y);
    if (!new_segment)
        return;

    new_segment->next = snake->head;
    snake->head = new_segment;
    if (!snake->tail){
        snake->tail = new_segment;
    }
    snake->length++;
}

void snake_remove_tail(Snake *snake){
    if (!snake->tail){
        return; // List is empty
    }
    if (snake->head == snake->tail){
        free(snake->tail);
        snake->tail = NULL;
        snake->head = NULL;
        snake->length--;
        return;
    }
    Segment *current = snake->head;
    while (current->next != snake->tail){
        current = current->next;
    }

    free(snake->tail);
    snake->tail = current;
    snake->tail->next = NULL;
    snake->length--;
}

void snake_free(Snake *snake){
    Segment *current = snake->head;
    while (current != NULL){
        Segment *next = current->next;
        free(current);
        current = next;
    }
    snake->head = NULL;
    snake->tail = NULL;
    snake->length = 0;
}

void iq_push(InputQueue *q, int dx, int dy){
    if (q->count >= INPUT_QUEUE_SIZE)
        return; // drop input when full
    
    q->dx[q->tail] = dx;
    q->dy[q->tail] = dy;
    q->tail = (q->tail + 1) % INPUT_QUEUE_SIZE;
    q->count++;
}

int iq_pop(InputQueue *q, int *dx, int *dy){
    if (q->count == 0)
        return 0;
    
    *dx = q->dx[q->head];
    *dy = q->dy[q->head];
    q->head = (q->head + 1) % INPUT_QUEUE_SIZE;
    q->count--;
    return 1;
}

void reset_snake(Snake *snake){
    snake_free(snake);
    snake->dx = 1;
    snake->dy = 0;
    snake->next_dx = 1;
    snake->next_dy = 0;

    snake_add_head(snake, GRID_COLS / 2 - 2, GRID_ROWS / 2); // tail
    snake_add_head(snake, GRID_COLS / 2 - 1, GRID_ROWS / 2); // middle
    snake_add_head(snake, GRID_COLS / 2, GRID_ROWS / 2);     // head
}

void reset_snake_rand(Snake *snake){
    snake_free(snake);
    int x = rand() % (GRID_COLS - 6) + 3;
    int y = rand() % (GRID_ROWS - 6) + 3;

    switch (rand() % 4){
    case 0: // pointing right
        snake->dx = 1;
        snake->dy = 0;
        snake->next_dx = 1;
        snake->next_dy = 0;
        snake_add_head(snake, x - 2, y); // tail
        snake_add_head(snake, x - 1, y); // middle
        snake_add_head(snake, x, y);     // head
        break;
    case 1: // pointing left
        snake->dx = -1;
        snake->dy = 0;
        snake->next_dx = -1;
        snake->next_dy = 0;
        snake_add_head(snake, x + 2, y); // tail
        snake_add_head(snake, x + 1, y); // middle
        snake_add_head(snake, x, y);     // head
        break;
    case 2: // pointing down
        snake->dx = 0;
        snake->dy = 1;
        snake->next_dx = 0;
        snake->next_dy = 1;
        snake_add_head(snake, x, y - 2); // tail
        snake_add_head(snake, x, y - 1); // middle
        snake_add_head(snake, x, y);     // head
        break;
    case 3: // pointing up
        snake->dx = 0;
        snake->dy = -1;
        snake->next_dx = 0;
        snake->next_dy = -1;
        snake_add_head(snake, x, y + 2); // tail
        snake_add_head(snake, x, y + 1); // middle
        snake_add_head(snake, x, y);     // head
        break;
    }
}

void freeAllSnakes(){
    for(int i=0; i<snakes.num; i++){
        free(snakes.snake[i].name);
        snake_free(&snakes.snake[i]); 
    }
    if (snakes.snake){
        free(snakes.snake);
        snakes.snake = NULL;
        snakes.num = 0;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SNAKE init
// ─────────────────────────────────────────────────────────────────────────────

void createSnake(const char *name, u_int8_t r, u_int8_t g, u_int8_t b, u_int8_t state){
    snakes.num++;
    Snake *temp = realloc(snakes.snake, snakes.num * sizeof(Snake));
    if (temp == NULL){
        printf("Memory allocation failed!\n");
        snakes.num--;
        return;
    }
    snakes.snake = temp;

    snakes.snake[snakes.num - 1].head = NULL;
    snakes.snake[snakes.num - 1].tail = NULL;
    snakes.snake[snakes.num - 1].length = 0;
    snakes.snake[snakes.num - 1].r = r;
    snakes.snake[snakes.num - 1].g = g;
    snakes.snake[snakes.num - 1].b = b;
    snakes.snake[snakes.num - 1].score = 0;
    snakes.snake[snakes.num - 1].lives = 3;
    snakes.snake[snakes.num - 1].name = strdup(name);
    snakes.snake[snakes.num - 1].state = state;

    reset_snake_rand(&snakes.snake[snakes.num - 1]);
}

// ─────────────────────────────────────────────────────────────────────────────
// SNAKE AI
// ─────────────────────────────────────────────────────────────────────────────

void reset_snake_AI(Snake *snake){
    snake_free(snake);
    snake->dx = -1;
    snake->dy = 0;
    snake->next_dx = -1;
    snake->next_dy = 0;

    snake_add_head(snake, GRID_COLS + 2, GRID_ROWS - 1); // tail
    snake_add_head(snake, GRID_COLS, GRID_ROWS - 1);     // middle
    snake_add_head(snake, GRID_COLS - 1, GRID_ROWS - 1); // head
}

void set_AI_down(Snake *snake){
    snake->next_dx = 0;
    snake->next_dy = 1;
}
void set_AI_up(Snake *snake){
    snake->next_dx = 0;
    snake->next_dy = -1;
}
void set_AI_right(Snake *snake){
    snake->next_dx = 1;
    snake->next_dy = 0;
}
void set_AI_left(Snake *snake){
    snake->next_dx = -1;
    snake->next_dy = 0;
}

void snake_AI_brain(Snake *snake) {
    int dxfood = food.x - snake->head->x; //pos -> food right
    int dyfood = food.y - snake->head->y; //pos -> food down

    u_int8_t canMove = 0;
    canMove += !(snake->dy ==  1 || has_hard_collision(snake->head->x,snake->head->y-1)) <<0; // facing down or collision   UP
    canMove += !(snake->dy == -1 || has_hard_collision(snake->head->x,snake->head->y+1)) <<1; // facing up or collision     DOWN
    canMove += !(snake->dx ==  1 || has_hard_collision(snake->head->x-1,snake->head->y)) <<2; // facing right or collision  LEFT
    canMove += !(snake->dx == -1 || has_hard_collision(snake->head->x+1,snake->head->y)) <<3; // facing left or collision   RIGHT

    if(abs(dyfood) > abs(dxfood)){
        if(dyfood > 0){                                 // want down
            if(canMove & CAN_DOWN)                      set_AI_down(snake);
            else if(dxfood > 0){                        // want right
                if(canMove & CAN_RIGHT)                 set_AI_right(snake);
                else if(canMove & CAN_LEFT)             set_AI_left(snake);
                else                                    set_AI_up(snake); 
            }
            else{                                       // want left
                if(canMove & CAN_LEFT)                  set_AI_left(snake);
                else if(canMove & CAN_RIGHT)            set_AI_right(snake);
                else                                    set_AI_up(snake); 
            }
        }
        else{                                           // want up
            if(canMove & CAN_UP)                        set_AI_up(snake);
            else if(dxfood > 0){                        // want right
                if(canMove & CAN_RIGHT)                 set_AI_right(snake);
                else if(canMove & CAN_LEFT)             set_AI_left(snake);
                else                                    set_AI_down(snake);
            }
            else{                                       // want left
                if(canMove & CAN_LEFT)                  set_AI_left(snake);
                else if(canMove & CAN_RIGHT)            set_AI_right(snake);
                else                                    set_AI_down(snake); 
            }
        }
    }
    else{
        if(dxfood > 0){                                 // want right
            if(canMove & CAN_RIGHT)                     set_AI_right(snake);
            else if(dyfood > 0){                        // want down
                if(canMove & CAN_DOWN)                  set_AI_down(snake);
                else if(canMove & CAN_UP)               set_AI_up(snake);
                else                                    set_AI_left(snake); 
            }
            else{                                       // want up
                if(canMove & CAN_UP)                    set_AI_up(snake);
                else if(canMove & CAN_DOWN)             set_AI_down(snake);
                else                                    set_AI_left(snake); 
            }
        }
        else{                                           // want left
            if(canMove & CAN_LEFT)                      set_AI_left(snake);
            else if(dyfood > 0){                        // want down
                if(canMove & CAN_DOWN)                  set_AI_down(snake);
                else if(canMove & CAN_UP)               set_AI_up(snake);
                else                                    set_AI_right(snake); 
            }
            else{                                       // want up
                if(canMove & CAN_UP)                    set_AI_up(snake);
                else if(canMove & CAN_DOWN)             set_AI_down(snake);
                else                                    set_AI_right(snake); 
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Spawning
// ─────────────────────────────────────────────────────────────────────────────

u_int8_t has_collision(int x, int y){
    u_int8_t collision = has_hard_collision(x, y);

    // food collisions
    if (food.is_active && x == food.x && y == food.y)
        collision = 1;

    // pwrups collisions
    for (int i = 0; i < pwrUps.num; i++){
        if (pwrUps.pwrUp[i].x == x && pwrUps.pwrUp[i].y == y){
            collision = 1;
            break;
        }
    }
    return collision;
}

u_int8_t has_hard_collision(int x, int y){
    u_int8_t collision = 0;

    // snake collisions
    for (int i = 0; i < snakes.num; i++){
        Segment *current = snakes.snake[i].head;
        while (current != NULL){
            if (!(snakes.snake[i].state & SNAKE_DEAD) && x == current->x && y == current->y){
                collision = 1;
                break;
            }
            current = current->next;
        }
    }

    // wall collisions
    for (int i = 0; i < walls.num; i++){
        if (walls.wall[i].x == x && walls.wall[i].y == y){
            collision = 1;
            break;
        }
    }
    return collision;
}

Point rand_no_collition(){
    Point p;
    p.x = rand() % GRID_COLS;
    p.y = rand() % GRID_ROWS;

    while (has_collision(p.x, p.y)){
        p.x = rand() % GRID_COLS;
        p.y = rand() % GRID_ROWS;
    }
    return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// Food
// ─────────────────────────────────────────────────────────────────────────────

void spawn_food(void){
    Point p = rand_no_collition();
    food.x = p.x;
    food.y = p.y;
    food.is_active = 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Power Ups
// ─────────────────────────────────────────────────────────────────────────────

void shrinkPowerUp(Snake *snake){
    if (snake->head->next != snake->tail)
        snake_remove_tail(snake);
}

void slowPowerUp(){
    state.speed_ms += 20;
}

void wallBreakPowerUp(){
    walls.num -= 10;
    if (walls.num > 0){
        Wall *temp = realloc(walls.wall, walls.num * sizeof(Wall));
        if (temp)
            walls.wall = temp;
    }
    else{
        free(walls.wall);
        walls.wall = NULL;
        walls.num = 0;
    }
}

void duplicatePowerUp(){
    createSnake("AI", rand() % 195, rand() % 195, rand() % 195, SNAKE_AI);
}

PwrUpType *init_shrinkPowerUp(){
    if (!(pwrUps.init & PWR_SHRINK)){
        pwrUps.shrink = malloc(sizeof(PwrUpType));
        pwrUps.shrink->r = 100;
        pwrUps.shrink->g = 225;
        pwrUps.shrink->b = 100;
        pwrUps.shrink->func = shrinkPowerUp;
        pwrUps.init |= PWR_SHRINK;
    }
    return pwrUps.shrink;
}

PwrUpType *init_slowPowerUp(){
    if (!(pwrUps.init & PWR_SLOW)){
        pwrUps.slow = malloc(sizeof(PwrUpType));
        pwrUps.slow->r = 100;
        pwrUps.slow->g = 150;
        pwrUps.slow->b = 225;
        pwrUps.slow->func = slowPowerUp;
        pwrUps.init |= PWR_SLOW;
    }
    return pwrUps.slow;
}

PwrUpType *init_wallBreakPowerUp(){
    if (!(pwrUps.init & PWR_WALLBREAK)){
        pwrUps.wallBreak = malloc(sizeof(PwrUpType));
        pwrUps.wallBreak->r = 225;
        pwrUps.wallBreak->g = 130;
        pwrUps.wallBreak->b = 000;
        pwrUps.wallBreak->func = wallBreakPowerUp;
        pwrUps.init |= PWR_WALLBREAK;
    }
    return pwrUps.wallBreak;
}

PwrUpType *init_duplicatePowerUp(){
    if (!(pwrUps.init & PWR_DUPLICATE)){
        pwrUps.duplicate = malloc(sizeof(PwrUpType));
        pwrUps.duplicate->r = 225;
        pwrUps.duplicate->g = 225;
        pwrUps.duplicate->b = 225;
        pwrUps.duplicate->func = duplicatePowerUp;
        pwrUps.init |= PWR_DUPLICATE;
    }
    return pwrUps.duplicate;
}

void createPwrUp(PwrUpType *pwrUpType, int x, int y){
    pwrUps.num++;
    PwrUp *temp = realloc(pwrUps.pwrUp, pwrUps.num * sizeof(PwrUp));
    if (temp == NULL){
        printf("Memory allocation failed!\n");
        pwrUps.num--;
        return;
    }
    pwrUps.pwrUp = temp;
    pwrUps.pwrUp[pwrUps.num - 1].x = x;
    pwrUps.pwrUp[pwrUps.num - 1].y = y;
    pwrUps.pwrUp[pwrUps.num - 1].type = pwrUpType;
}

void remove_PwrUps(){
    free(pwrUps.pwrUp);
    pwrUps.pwrUp = NULL;
    pwrUps.num = 0;

    free(pwrUps.shrink);    pwrUps.shrink   = NULL;
    free(pwrUps.slow);      pwrUps.slow     = NULL;
    free(pwrUps.wallBreak); pwrUps.wallBreak = NULL;
    free(pwrUps.duplicate); pwrUps.duplicate = NULL;

    pwrUps.init = 0;
}

void spawn_powerup(u_int8_t type){
    Point p = rand_no_collition();

    if (type == PWR_SLOW){
        PwrUpType *type = init_slowPowerUp();
        createPwrUp(type, p.x, p.y);
    }
    else if (type == PWR_SHRINK){
        PwrUpType *type = init_shrinkPowerUp();
        createPwrUp(type, p.x, p.y);
    }
    else if (type == PWR_WALLBREAK){
        PwrUpType *type = init_wallBreakPowerUp();
        createPwrUp(type, p.x, p.y);
    }
    else if (type == PWR_DUPLICATE){
        PwrUpType *type = init_duplicatePowerUp();
        createPwrUp(type, p.x, p.y);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Walls
// ─────────────────────────────────────────────────────────────────────────────

void spawn_wall(int x, int y){
    walls.num++;
    Wall *temp = realloc(walls.wall, walls.num * sizeof(Wall));
    if (temp == NULL){
        printf("Memory allocation failed!\n");
        walls.num--;
        return;
    }
    walls.wall = temp;
    walls.wall[walls.num - 1].x = x;
    walls.wall[walls.num - 1].y = y;
}

void spawn_walls_rand(int num){
    for (int i = 0; i < num; i++){
        Point p = rand_no_collition();
        spawn_wall(p.x, p.y);
    }
}

void remove_walls(){
    free(walls.wall);
    walls.wall = NULL;
    walls.num = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Menu
// ─────────────────────────────────────────────────────────────────────────────
MenuItem *create_menu_item(const char *label, SDL_Keycode hotkey, void (*action)(void), int y_offset){
    MenuItem *item = malloc(sizeof(MenuItem));
    strcpy(item->label, label);
    strcpy(item->curlabel, ">>> ");
    strcat(item->curlabel, label);
    strcat(item->curlabel, " <<<");
    item->hotkey = hotkey;
    item->action = action;
    item->y_offset = y_offset;
    item->next = NULL;
    item->prev = NULL;
    return item;
}

void free_menu(MenuItem *head){
    if (!head)
        return;
    MenuItem *cur = head;
    do{
        MenuItem *next = cur->next; // save next BEFORE freeing cur
        free(cur);                  // free current node
        cur = next;                 // advance
    } while (cur != head); // stop when we've looped back to start
}

void buildMenu(){ //pause menu
    free_menu(menu_head);
    menu_head = NULL;
    selected_menu = NULL;

    MenuItem *resume = create_menu_item("-- RESUME --", SDLK_p, resume_action, 40);
    MenuItem *restart = create_menu_item("-- RESTART --", SDLK_r, restart_action, 60);
    MenuItem *high_score = create_menu_item("-- HIGH SCORES --", SDLK_h, scores_action, 80);
    MenuItem *start_screen = create_menu_item("-- START SCREEN --", SDLK_m, return_to_start_screen_action, 100);
    MenuItem *settings = create_menu_item("-- SETTINGS --", SDLK_s, settings_action, 120);
    resume->next = restart;
    restart->prev = resume;
    restart->next = high_score;
    high_score->prev = restart;
    high_score->next = start_screen;
    start_screen->prev = high_score;
    start_screen->next = settings;
    settings->prev = start_screen;
    settings->next = resume;
    resume->prev = settings;

    menu_head = resume;
    selected_menu = resume;
}

void buildStartMenu(){ //start menu
    free_menu(start_menu_head);
    start_menu_head = NULL;
    selected_start_menu = NULL;

    MenuItem *play = create_menu_item("-- Start --", SDLK_RETURN, play_action, 60);
    MenuItem *settings_start = create_menu_item("-- SETTINGS --", SDLK_s, settings_action, 80);
    MenuItem *multiPlayer = create_menu_item("-- MULTIPLAYER --",SDLK_m, multiPlayer_action, 100);
    MenuItem *exit1 = create_menu_item("-- EXIT --", SDLK_ESCAPE, exit_action, 120);
    play->next = settings_start;
    settings_start->prev = play;
    settings_start->next = multiPlayer;
    multiPlayer->prev = settings_start;
    multiPlayer->next = exit1;
    exit1->prev = multiPlayer;
    exit1->next = play;
    play->prev = exit1;

    start_menu_head = play;
    selected_start_menu = play;
}

void buildSettingsMenu(){ //setting menu
    free_menu(setting_menu_head);
    setting_menu_head = NULL;
    selected_setting_menu = NULL;

    const char *wall_wrap_label = (state.flags & FLAG_WALL_WRAP) ? "-- WALL WRAP (ON) --" : "-- WALL WRAP (OFF) --";
    const char *walls_label = (state.flags & FLAG_WALLS) ? "-- SPAWN WALLS (ON) --" : "-- SPAWN WALLS (OFF) --";
    const char *pwrups_label = (state.flags & FLAG_PWRUPS) ? "-- SPAWN POWER UPS (ON) --" : "-- SPAWN POWER UPS (OFF) --";

    MenuItem *brightness = create_menu_item("-- BRIGHTNESS --", SDLK_s, brightness_action, 60);
    MenuItem *wallWrap = create_menu_item(wall_wrap_label, SDLK_s, wall_wrap_action, 80);
    MenuItem *spawnWalls = create_menu_item(walls_label, SDLK_s, spawn_walls_action, 100);
    MenuItem *spawnPwrUps = create_menu_item(pwrups_label, SDLK_s, spawn_pwrups_action, 120);
    MenuItem *back = create_menu_item("-- BACK --", SDLK_ESCAPE, back_action, 140);
    brightness->next = wallWrap;
    wallWrap->prev = brightness;
    wallWrap->next = spawnWalls;
    spawnWalls->prev = wallWrap;
    spawnWalls->next = spawnPwrUps;
    spawnPwrUps->prev = spawnWalls;
    spawnPwrUps->next = back;
    back->prev = spawnPwrUps;
    back->next = brightness;
    brightness->prev = back;

    setting_menu_head = brightness;
    selected_setting_menu = brightness;
}

void buildMultiSnakeMenu(){
    free_menu(multi_player_menu_head);
    multi_player_menu_head = NULL;
    selected_multi_player_menu = NULL;

    char add_AI_label[32];
    snprintf(add_AI_label, sizeof(add_AI_label), "-- ADD AI SNAKE (%d) --", state.flags/FLAG_SNAKE_AI);
    char remove_AI_label[32];
    snprintf(remove_AI_label, sizeof(remove_AI_label), "-- REMOVE AI SNAKE (%d) --", state.flags/FLAG_SNAKE_AI);
    const char *p2_label = (state.flags & FLAG_SNAKE_P2) ? "-- PLAYER 2 (ON) --" : "-- PLAYER 2 (OFF) --";
    const char *p3_label = (state.flags & FLAG_SNAKE_P3) ? "-- PLAYER 3 (ON) --" : "-- PLAYER 3 (OFF) --";
    MenuItem *p2 = create_menu_item(p2_label, SDLK_s, p2_action, 60);
    MenuItem *p3 = create_menu_item(p3_label, SDLK_s, p3_action, 80);
    MenuItem *addAI = create_menu_item(add_AI_label, SDLK_s, add_AI_action, 100);
    MenuItem *removeAI = create_menu_item(remove_AI_label, SDLK_s, remove_AI_action, 120);
    MenuItem *back = create_menu_item("-- BACK --", SDLK_ESCAPE, back_to_start_action, 140);
    
    p2->next = p3;
    p3->prev = p2;
    p3->next = addAI;
    addAI->prev = p3;
    addAI->next = removeAI;
    removeAI->prev = addAI;
    removeAI->next = back;
    back->prev = removeAI;
    back->next = p2;
    p2->prev = back;

    multi_player_menu_head = p2;
    selected_multi_player_menu = p2;
}

// ─────────────────────────────────────────────────────────────────────────────
// Menu Actions
// ─────────────────────────────────────────────────────────────────────────────

void resume_action(){
    printf("Resuming game...\n");
    state.flags &= ~FLAG_PAUSED;
}

void restart_action(){
    printf("Restarting game...\n");

    // Reinitialize the game
    init_game();

    // Unpause the game
    state.flags &= ~FLAG_PAUSED;
}

void scores_action(){
    printf("Displaying high scores...\n");
    high_scores();
}

// ─────────────────────────────────────────────────────────────────────────────
// Start Menu Actions
// ─────────────────────────────────────────────────────────────────────────────

void start_screen_action(){
    init_game();
    state.flags |= FLAG_START_SCREEN;
    state.flags &= ~FLAG_PAUSED;
    selected_start_menu = start_menu_head;
}

void play_action(){
    printf("Starting game...\n");

    // Reinitialize the game
    init_game();

    // Unpause the game
    state.flags &= ~FLAG_PAUSED;
    state.flags &= ~FLAG_START_SCREEN;
}

void return_to_start_screen_action(){
    state.flags |= FLAG_START_SCREEN;
    state.flags |= FLAG_PAUSED;
}

void exit_action(){
    printf("Exiting game...\n");
    state.flags |= FLAG_GAME_OVER;
}

// ─────────────────────────────────────────────────────────────────────────────
// Settings Actions
// ─────────────────────────────────────────────────────────────────────────────

void settings_action(){
    printf("Opening settings menu...\n");
    settings_opened_from = (state.flags & FLAG_START_SCREEN) ? 0 : 1;
    state.flags |= FLAG_SETTINGS;
    state.flags |= FLAG_PAUSED;
    state.flags &= ~FLAG_START_SCREEN;
    selected_setting_menu = setting_menu_head;
}

void back_action(){
    state.flags &= ~FLAG_SETTINGS;
    if (settings_opened_from == 1)
        state.flags |= FLAG_PAUSED; // return to pause menu
    else
        state.flags |= FLAG_START_SCREEN; // return to start menu
}

void brightness_action(){
    brightness_level = (brightness_level + 1) % 3;

}

void wall_wrap_action(){
    state.flags ^= FLAG_WALL_WRAP;

    MenuItem *item = selected_setting_menu;
    const char *label = (state.flags & FLAG_WALL_WRAP) ? "-- WALL WRAP (ON) --" : "-- WALL WRAP (OFF) --";
    strcpy(item->label, label);
    strcpy(item->curlabel, ">>> ");
    strcat(item->curlabel, label);
    strcat(item->curlabel, " <<<");
}

void spawn_walls_action(){
    state.flags ^= FLAG_WALLS;

    MenuItem *item = selected_setting_menu;
    const char *label = (state.flags & FLAG_WALLS) ? "-- SPAWN WALLS (ON) --" : "-- SPAWN WALLS (OFF) --";
    strcpy(item->label, label);
    strcpy(item->curlabel, ">>> ");
    strcat(item->curlabel, label);
    strcat(item->curlabel, " <<<");
}

void spawn_pwrups_action(){
    state.flags ^= FLAG_PWRUPS;

    MenuItem *item = selected_setting_menu;
    const char *label = (state.flags & FLAG_PWRUPS) ? "-- SPAWN POWER UPS (ON) --" : "-- SPAWN POWER UPS (OFF) --";
    strcpy(item->label, label);
    strcpy(item->curlabel, ">>> ");
    strcat(item->curlabel, label);
    strcat(item->curlabel, " <<<");
}


// ─────────────────────────────────────────────────────────────────────────────
// Multiplayer Actions
// ─────────────────────────────────────────────────────────────────────────────

void multiPlayer_action(){
    printf("Opening multiplayer menu...\n");
    state.flags |= FLAG_MULTISNAKE_SCREEN;
    state.flags |= FLAG_PAUSED;
    state.flags &= ~FLAG_START_SCREEN;
    selected_multi_player_menu = multi_player_menu_head;
}

void p2_action(){
    state.flags ^= FLAG_SNAKE_P2;

    MenuItem *item = selected_multi_player_menu;
    const char *label = (state.flags & FLAG_SNAKE_P2) ? "-- PLAYER 2 (ON) --" : "-- PLAYER 2 (OFF) --";
    strcpy(item->label, label);
    strcpy(item->curlabel, ">>> ");
    strcat(item->curlabel, label);
    strcat(item->curlabel, " <<<");
}

void p3_action(){
    state.flags ^= FLAG_SNAKE_P3;

    MenuItem *item = selected_multi_player_menu;
    const char *label = (state.flags & FLAG_SNAKE_P3) ? "-- PLAYER 3 (ON) --" : "-- PLAYER 3 (OFF) --";
    strcpy(item->label, label);
    strcpy(item->curlabel, ">>> ");
    strcat(item->curlabel, label);
    strcat(item->curlabel, " <<<");
}


void add_AI_action(){
    state.flags += FLAG_SNAKE_AI;

    MenuItem *item = selected_multi_player_menu;
    char label[32];
    snprintf(label, sizeof(label), "-- ADD AI SNAKE (%d) --", state.flags/FLAG_SNAKE_AI);
    strcpy(item->label, label);
    strcpy(item->curlabel, ">>> ");
    strcat(item->curlabel, label);
    strcat(item->curlabel, " <<<");

    item = item->next;
    snprintf(label, sizeof(label), "-- REMOVE AI SNAKE (%d) --", state.flags/FLAG_SNAKE_AI);
    strcpy(item->label, label);
    strcpy(item->curlabel, ">>> ");
    strcat(item->curlabel, label);
    strcat(item->curlabel, " <<<");
}

void remove_AI_action(){
    if(state.flags/FLAG_SNAKE_AI > 0){
        state.flags -= FLAG_SNAKE_AI;

        MenuItem *item = selected_multi_player_menu;
        char label[32];
        snprintf(label, sizeof(label), "-- REMOVE AI SNAKE (%d) --", state.flags/FLAG_SNAKE_AI);
        strcpy(item->label, label);
        strcpy(item->curlabel, ">>> ");
        strcat(item->curlabel, label);
        strcat(item->curlabel, " <<<");

        item = item->prev;
        snprintf(label, sizeof(label), "-- ADD AI SNAKE (%d) --", state.flags/FLAG_SNAKE_AI);
        strcpy(item->label, label);
        strcpy(item->curlabel, ">>> ");
        strcat(item->curlabel, label);
        strcat(item->curlabel, " <<<");
    }
}

void back_to_start_action(){
    state.flags &= ~FLAG_MULTISNAKE_SCREEN;
    state.flags |= FLAG_START_SCREEN;
}


// ─────────────────────────────────────────────────────────────────────────────
// Persistent High Score
// ─────────────────────────────────────────────────────────────────────────────

void high_scores(){

    FILE *leaderboard = fopen("leaderboard.txt", "r");
    if (leaderboard == NULL){
        // only runs if file doesn't exist
        leaderboard = fopen("leaderboard.txt", "w");
        fprintf(leaderboard, "0 \n0 \n0");
        fclose(leaderboard);
        leaderboard = fopen("leaderboard.txt", "r");
    }

    // makming temp array to store leaderboard and current score
    int *temp_leaderboard = malloc(5 * sizeof(int));
    temp_leaderboard[3] = snakes.snake[0].score;

    // get leaderboard
    int i = 0;
    while (fscanf(leaderboard, "%d", &temp_leaderboard[i]) == 1 && i < 4){
        i++;
    }
    fclose(leaderboard);

    // update leaderboard
    for (int j = 0; j < i; j++){
        if (temp_leaderboard[3] > temp_leaderboard[j]){
            temp_leaderboard[4] = temp_leaderboard[j];
            temp_leaderboard[j] = temp_leaderboard[3];
            temp_leaderboard[3] = temp_leaderboard[4];
        }
    }

    // update leaderboard file
    leaderboard = fopen("leaderboard.txt", "w");
    for (int j = 0; j < i; j++){
        fprintf(leaderboard, "%d\n", temp_leaderboard[j]);
    }
    fclose(leaderboard);

    // print updated leaderboard
    printf("----------------\nTop Scores:\n");
    printf("First: %d \nSecond: %d \nThird: %d", temp_leaderboard[0], temp_leaderboard[1], temp_leaderboard[2]);
    printf("\n----------------\n");

    // free the array we used to store the leaderboard scores
    free(temp_leaderboard);
}

// ─────────────────────────────────────────────────────────────────────────────
// Initialisation
// ─────────────────────────────────────────────────────────────────────────────

void init_game(){
    srand((unsigned int)SDL_GetTicks());
    state.score = 0;
    state.lives = 3;
    state.speed_ms = 120;

    remove_walls();
    remove_PwrUps();
    freeAllSnakes();

    snakes.num = 0;

    createSnake("P1", 50, 175, 50, 0);

    if (state.flags & FLAG_SNAKE_P2)
        createSnake("P2", 50, 50, 175, 0);
    if (state.flags & FLAG_SNAKE_P3)
        createSnake("P3", 175, 175, 50, 0);

    int num_snakeAIs = state.flags / FLAG_SNAKE_AI;
    for (int i = 0; i < num_snakeAIs; i++){
        char name[8];
        snprintf(name, sizeof(name), "AI%d", i);
        createSnake(name, rand() % 195, rand() % 195, rand() % 195, SNAKE_AI);
    }

    walls.wall = NULL;
    walls.num = 0;

    pwrUps.pwrUp = NULL;
    pwrUps.num = 0;
    pwrUps.init = 0;

    input_queue.head = input_queue.tail = input_queue.count = 0;
    buildMenu();
    buildStartMenu();
    buildSettingsMenu();
    buildMultiSnakeMenu();

    spawn_walls_rand(0);
    spawn_food();
}


// ─────────────────────────────────────────────────────────────────────────────
// Game loop
// ─────────────────────────────────────────────────────────────────────────────
void start_game(SDL_Renderer *renderer){
    state.flags = 0 + FLAG_START_SCREEN + FLAG_PAUSED + 0 * FLAG_SNAKE_AI; // + FLAG_WALL_WRAP;
    init_game();
    Uint32 last_update = SDL_GetTicks();

    state.flags |= FLAG_START_SCREEN;
    state.flags |= FLAG_PAUSED;

    while (!(state.flags & FLAG_GAME_OVER)){

        handle_input(&state, &snakes);

        Uint32 now = SDL_GetTicks();
        if (now - last_update >= (Uint32)state.speed_ms){
            if (!(state.flags & FLAG_PAUSED)){
                for (int i = 0; i < snakes.num; i++){
                    if (snakes.snake[i].state & SNAKE_AI)
                        snake_AI_brain(&snakes.snake[i]);

                    update_physics(&snakes.snake[i], &food, &pwrUps, &state);
                }
            }
            last_update = now;
        }
        render_game(renderer, &snakes, &food, &walls, &pwrUps, &state);
        SDL_Delay(10);
    }

    // TODO: display a game-over screen and wait for a restart or quit input.
    //       On restart: call init_game() and re-enter the loop.
    printf("\n\n\nGame Over! Final score: %d\n", state.score);
    high_scores();

    remove_walls();
    remove_PwrUps();
    freeAllSnakes();

    free_menu(menu_head);
    free_menu(start_menu_head);
    free_menu(setting_menu_head);
    free_menu(multi_player_menu_head);

    render_cleanup();
}
