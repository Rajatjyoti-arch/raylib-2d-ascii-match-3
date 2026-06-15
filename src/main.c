#include "raylib.h"
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

#define BOARD_SIZE 8
#define TILE_SIZE 42
#define TILE_TYPES 5
#define MAX_SCORE_POPUPS 100

const char tile_chars[TILE_TYPES] = { '#', '@', '$', '%', '&' };

char board[BOARD_SIZE][BOARD_SIZE];
bool matched[BOARD_SIZE][BOARD_SIZE] = {0};
float fall_offset[BOARD_SIZE][BOARD_SIZE] = {0};

int score = 0;
Vector2 grid_origin;
Texture2D background;
Vector2 selected_tile = { -1, -1 };
float fall_speed = 8.0f;
float match_delay_timer = 0.0f;
const float MATCH_DELAY_DURATION = 0.2f;

float score_scale = 1.0f;
float score_scale_velocity = 0.0f;
bool score_animating = false;

Music background_music;
Sound match_sound;

typedef enum {
	STATE_IDLE,
	STATE_ANIMATING,
	STATE_MATCH_DELAY
} TileState;

TileState tile_state;

typedef struct {
    Vector2 position;
    int amount;
    float lifetime;
    bool active;
    float alpha;
} ScorePopup;

ScorePopup score_popups[MAX_SCORE_POPUPS] = {0};
char random_tile() {
	return tile_chars[rand() % TILE_TYPES];
}

void swap_tiles(int x1, int y1, int x2, int y2) {
    char temp = board[y1][x1];
    board[y1][x1] = board[y2][x2];
    board[y2][x2] = temp;
}

bool are_tiles_adjacent(Vector2 a, Vector2 b) {
    return (abs((int)a.x - (int)b.x) + abs((int)a.y - (int)b.y)) == 1;
}

void add_score_popup(int x, int y, int amount, Vector2 grid_origin) {
	for (int i = 0; i < MAX_SCORE_POPUPS; i++) {
		if (!score_popups[i].active) {
			score_popups[i].position = (Vector2) {
				grid_origin.x + x * TILE_SIZE + TILE_SIZE / 2,
				grid_origin.y + y * TILE_SIZE + TILE_SIZE / 2
			};
			score_popups[i].amount = amount;
			score_popups[i].lifetime = 1.0f;
			score_popups[i].active = true;
			break;
		}
	}
}


bool find_matches() {
	bool found = false;
	for (int y = 0; y < BOARD_SIZE; y++) {
		for (int x = 0; x < BOARD_SIZE; x++) {
			matched[y][x] = false;
		}
	}

	for (int y = 0; y < BOARD_SIZE; y++) {
		for (int x = 0; x < BOARD_SIZE - 2; x++) {
			char t = board[y][x];
			if (t == board[y][x + 1] &&
				t == board[y][x + 2]) {
				matched[y][x] = matched[y][x + 1] = matched[y][x + 2] = true;
				score += 10;
				found = true;
                PlaySound(match_sound);

                score_animating = true;
                score_scale = 2.0f;
                score_scale_velocity = -2.5f;

               add_score_popup(x, y,10, grid_origin);
			}
		}
	}

	for (int x = 0; x < BOARD_SIZE; x++) {
		for (int y = 0; y < BOARD_SIZE - 2; y++) {
			char t = board[y][x];
			if (t == board[y + 1][x] &&
				t == board[y + 2][x]) {
				matched[y][x] = matched[y + 1][x] = matched[y + 2][x] = true;
				score += 10;
				found = true;
                PlaySound(match_sound);

                score_animating = true;
                score_scale = 2.0f;
                score_scale_velocity = -2.5f;

                add_score_popup(x, y, 10, grid_origin);
			}
		}
	}

	return found;
}

// Check if any valid swap exists that would create a match
bool has_possible_moves() {
	// Try every horizontal adjacent swap
	for (int y = 0; y < BOARD_SIZE; y++) {
		for (int x = 0; x < BOARD_SIZE - 1; x++) {
			// Swap (x,y) and (x+1,y)
			char temp = board[y][x];
			board[y][x] = board[y][x + 1];
			board[y][x + 1] = temp;

			// Check if this swap creates any match
			bool creates_match = false;
			for (int cy = 0; cy < BOARD_SIZE && !creates_match; cy++) {
				for (int cx = 0; cx < BOARD_SIZE - 2 && !creates_match; cx++) {
					if (board[cy][cx] == board[cy][cx + 1] && board[cy][cx] == board[cy][cx + 2])
						creates_match = true;
				}
			}
			for (int cx = 0; cx < BOARD_SIZE && !creates_match; cx++) {
				for (int cy = 0; cy < BOARD_SIZE - 2 && !creates_match; cy++) {
					if (board[cy][cx] == board[cy + 1][cx] && board[cy][cx] == board[cy + 2][cx])
						creates_match = true;
				}
			}

			// Swap back
			board[y][x + 1] = board[y][x];
			board[y][x] = temp;

			if (creates_match) return true;
		}
	}

	// Try every vertical adjacent swap
	for (int y = 0; y < BOARD_SIZE - 1; y++) {
		for (int x = 0; x < BOARD_SIZE; x++) {
			char temp = board[y][x];
			board[y][x] = board[y + 1][x];
			board[y + 1][x] = temp;

			bool creates_match = false;
			for (int cy = 0; cy < BOARD_SIZE && !creates_match; cy++) {
				for (int cx = 0; cx < BOARD_SIZE - 2 && !creates_match; cx++) {
					if (board[cy][cx] == board[cy][cx + 1] && board[cy][cx] == board[cy][cx + 2])
						creates_match = true;
				}
			}
			for (int cx = 0; cx < BOARD_SIZE && !creates_match; cx++) {
				for (int cy = 0; cy < BOARD_SIZE - 2 && !creates_match; cy++) {
					if (board[cy][cx] == board[cy + 1][cx] && board[cy][cx] == board[cy + 2][cx])
						creates_match = true;
				}
			}

			board[y + 1][x] = board[y][x];
			board[y][x] = temp;

			if (creates_match) return true;
		}
	}

	return false;
}

// Shuffle the entire board randomly (used when no moves are possible)
void shuffle_board() {
	for (int y = 0; y < BOARD_SIZE; y++) {
		for (int x = 0; x < BOARD_SIZE; x++) {
			board[y][x] = random_tile();
		}
	}
}

void resolve_matches() {
	for (int x = 0; x < BOARD_SIZE; x++) {
		int write_y = BOARD_SIZE - 1;
		for (int y = BOARD_SIZE - 1; y >= 0; y--) {
			if (!matched[y][x]) {
				if (y != write_y) {
					board[write_y][x] = board[y][x];
					fall_offset[write_y][x] = (write_y - y) * TILE_SIZE;
					board[y][x] = ' ';
				}
				write_y--;
			}
		}

		while (write_y >= 0) {
			board[write_y][x] = random_tile();
			fall_offset[write_y][x] = (write_y + 1) * TILE_SIZE;
			write_y--;
		}
	}

	tile_state = STATE_ANIMATING;
}

void init_board() {
	int grid_width = BOARD_SIZE * TILE_SIZE;
	int grid_height = BOARD_SIZE * TILE_SIZE;

	grid_origin = (Vector2){
		(GetScreenWidth() - grid_width) / 2,
		(GetScreenHeight() - grid_height) / 2
	};

	// Keep generating boards until we get one with no
	// pre-existing matches AND at least one valid move
	int attempts = 0;
	do {
		shuffle_board();
		attempts++;

		// Clear any pre-existing matches by regenerating those tiles
		bool had_matches = true;
		while (had_matches) {
			had_matches = false;
			for (int y = 0; y < BOARD_SIZE; y++) {
				for (int x = 0; x < BOARD_SIZE; x++) {
					matched[y][x] = false;
				}
			}
			// Check horizontal
			for (int y = 0; y < BOARD_SIZE; y++) {
				for (int x = 0; x < BOARD_SIZE - 2; x++) {
					if (board[y][x] == board[y][x+1] && board[y][x] == board[y][x+2]) {
						matched[y][x] = matched[y][x+1] = matched[y][x+2] = true;
						had_matches = true;
					}
				}
			}
			// Check vertical
			for (int x = 0; x < BOARD_SIZE; x++) {
				for (int y = 0; y < BOARD_SIZE - 2; y++) {
					if (board[y][x] == board[y+1][x] && board[y][x] == board[y+2][x]) {
						matched[y][x] = matched[y+1][x] = matched[y+2][x] = true;
						had_matches = true;
					}
				}
			}
			// Replace matched tiles with new random ones
			if (had_matches) {
				for (int y = 0; y < BOARD_SIZE; y++) {
					for (int x = 0; x < BOARD_SIZE; x++) {
						if (matched[y][x]) {
							board[y][x] = random_tile();
							matched[y][x] = false;
						}
					}
				}
			}
		}
	} while (!has_possible_moves() && attempts < 1000);

	// Reset matched and fall_offset arrays for clean state
	for (int y = 0; y < BOARD_SIZE; y++) {
		for (int x = 0; x < BOARD_SIZE; x++) {
			matched[y][x] = false;
			fall_offset[y][x] = 0;
		}
	}

	tile_state = STATE_IDLE;
}

int main(void) {
    const int screen_width = 800;
    const int screen_height = 450;

	InitWindow(screen_width, screen_height, "GAME");
    SetTargetFPS(60);
    srand(time(NULL));

    InitAudioDevice();

    background = LoadTexture("assets/back.png");
	background_music = LoadMusicStream("assets/music.mp3");
	match_sound = LoadSound("assets/match.mp3");
	PlayMusicStream(background_music);
    init_board();
    Vector2 mouse = {0, 0};

    while(!WindowShouldClose()) {

        UpdateMusicStream(background_music);

		mouse = GetMousePosition();
		if (tile_state == STATE_IDLE && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
			int x = (mouse.x - grid_origin.x) / TILE_SIZE;
			int y = (mouse.y - grid_origin.y) / TILE_SIZE;
			if (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE) {
				Vector2 current_tile = (Vector2){ x, y };
				if (selected_tile.x < 0) {
					selected_tile = current_tile;
				}
				else {
					if (are_tiles_adjacent(selected_tile, current_tile)) {
						swap_tiles(selected_tile.x, selected_tile.y, current_tile.x, current_tile.y);
						if (find_matches()) {
							resolve_matches();
						}
						else {
							swap_tiles(selected_tile.x, selected_tile.y, current_tile.x, current_tile.y);
						}
					}
					selected_tile = (Vector2){ -1, -1 };
				}
			}
		}

		if (tile_state == STATE_ANIMATING) {
			bool still_animating = false;

			for (int y = 0; y < BOARD_SIZE; y++) {
				for (int x = 0; x < BOARD_SIZE; x++) {
					if (fall_offset[y][x] > 0) {
						fall_offset[y][x] -= fall_speed;
						if (fall_offset[y][x] < 0) {
							fall_offset[y][x] = 0;
						}
						else {
							still_animating = true;
						}

					}
				}
			}

			if (!still_animating) {
				tile_state = STATE_MATCH_DELAY;
				match_delay_timer = MATCH_DELAY_DURATION;
			}
		}

		if (tile_state == STATE_MATCH_DELAY) {
			match_delay_timer -= GetFrameTime();
			if (match_delay_timer <= 0.0f) {
				if (find_matches()) {
					resolve_matches();
				}
				else if (!has_possible_moves()) {
					// No moves left — reshuffle the board
					init_board();
				}
				else {
					tile_state = STATE_IDLE;
				}
			}
		}

        for (int i = 0; i < MAX_SCORE_POPUPS; i++) {
			if (score_popups[i].active) {
				score_popups[i].lifetime -= GetFrameTime();
                score_popups[i].position.y -= 30 * GetFrameTime();
                score_popups[i].alpha = score_popups[i].lifetime;

				if (score_popups[i].lifetime <= 0.0f) {
					score_popups[i].active = false;
				}
			}
		}

        if (score_animating) {
            score_scale += score_scale_velocity * GetFrameTime();
            if (score_scale <= 1.0f) {
                score_scale = 1.0f;
                score_animating = false;
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);

        DrawTexturePro(
            background,
            (Rectangle){
                0, 0, background.width, background.height
        },
            (Rectangle){
                0, 0, GetScreenWidth(), GetScreenHeight()
        },
            (Vector2) {
                0, 0
        },
            0.0f,
            WHITE
        );

        DrawRectangle(
			grid_origin.x,
			grid_origin.y,
			BOARD_SIZE* TILE_SIZE,
			BOARD_SIZE* TILE_SIZE,
			Fade(DARKGRAY, 0.60f)
		);

		for (int y = 0; y < BOARD_SIZE; y++) {
			for (int x = 0; x < BOARD_SIZE; x++) {
				Rectangle rect = {
					grid_origin.x + (x * TILE_SIZE),
					grid_origin.y + (y * TILE_SIZE),
					TILE_SIZE,
					TILE_SIZE
				};

				DrawRectangleLinesEx(rect, 1, DARKGRAY);

				if (board[y][x] != ' ') {
					DrawTextEx(
						GetFontDefault(),
						TextFormat("%c", board[y][x]),
						(Vector2) {
							rect.x + 12,
							rect.y + 8 - fall_offset[y][x]
						},
						20,
						1,
						matched[y][x] ? GREEN : WHITE
					);
				}
			}
		}

		if (selected_tile.x >= 0) {
			DrawRectangleLinesEx((Rectangle) {
				grid_origin.x + (selected_tile.x * TILE_SIZE),
					grid_origin.y + (selected_tile.y * TILE_SIZE),
					TILE_SIZE, TILE_SIZE
			}, 2, YELLOW);
		}

        DrawTextEx(GetFontDefault(), TextFormat("Score : %d", score), (Vector2){20, 20}, 24 * score_scale, 1, WHITE);

        for (int i = 0; i < MAX_SCORE_POPUPS; i++) {
            if (score_popups[i].active) {
                Color c = Fade(YELLOW, score_popups[i].alpha);
                DrawText(
                    TextFormat("+%d", score_popups[i].amount), score_popups[i].position.x, score_popups[i].position.y, 20, c);
            }
        }

        EndDrawing();
    }

    StopMusicStream(background_music);
    UnloadMusicStream(background_music);
    UnloadSound(match_sound);

    CloseAudioDevice();
    UnloadTexture(background);

    CloseWindow();
    return 0;
    
}
