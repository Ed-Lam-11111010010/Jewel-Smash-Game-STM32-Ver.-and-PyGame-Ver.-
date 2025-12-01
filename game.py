import pygame
import random

# Initialize Pygame
pygame.init()

# Screen dimensions and grid settings
WIDTH, HEIGHT = 800, 600
TILE_SIZE = 64
GRID_SIZE = 8
MARGIN = 50  # Space around the grid

# Colors for normal tiles
NORMAL_COLORS = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (255, 165, 0)]  # Red, Green, Blue, Yellow, Orange

# Special tile types
HORIZONTAL_CLEARER = "HORIZONTAL_CLEARER"
VERTICAL_CLEARER = "VERTICAL_CLEARER"
BOMB = "BOMB"

# Special tile colors
SPECIAL_COLORS = {
    HORIZONTAL_CLEARER: (200, 200, 255),  # Light blue
    VERTICAL_CLEARER: (200, 255, 200),    # Light green
    BOMB: (255, 200, 200),                # Light red
}

# Background and highlight colors
BACKGROUND_COLOR = (30, 30, 30)
HIGHLIGHT_COLOR = (255, 255, 255)

# Initialize screen
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Tile Matching Game")

# Clock for controlling frame rate
clock = pygame.time.Clock()
FPS = 60

# Create a grid
grid = [[random.choice(NORMAL_COLORS) for _ in range(GRID_SIZE)] for _ in range(GRID_SIZE)]

# Selected tile
selected_tile = None

# Score tracking
score = 0


def draw_grid():
    """Draws the grid of tiles on the screen."""
    for y in range(GRID_SIZE):
        for x in range(GRID_SIZE):
            tile = grid[y][x]
            if tile is not None:
                if isinstance(tile, tuple):  # Normal tile (color tuple)
                    color = tile
                else:  # Special tile
                    color = SPECIAL_COLORS[tile]

                # Draw the tile
                pygame.draw.rect(
                    screen,
                    color,
                    (MARGIN + x * TILE_SIZE, MARGIN + y * TILE_SIZE, TILE_SIZE, TILE_SIZE),
                )

                # Add a symbol for special tiles
                if tile == HORIZONTAL_CLEARER:
                    draw_symbol(x, y, "<-->")
                elif tile == VERTICAL_CLEARER:
                    draw_symbol(x, y, "^  v")
                elif tile == BOMB:
                    draw_symbol(x, y, "*")

            # Add a border for each tile
            pygame.draw.rect(
                screen,
                BACKGROUND_COLOR,
                (MARGIN + x * TILE_SIZE, MARGIN + y * TILE_SIZE, TILE_SIZE, TILE_SIZE),
                2,
            )


def draw_symbol(x, y, symbol):
    """Draws a symbol in the center of a tile."""
    font = pygame.font.SysFont(None, 30)  # Font for symbols
    text = font.render(symbol, True, (0, 0, 0))  # Black text
    text_rect = text.get_rect(center=(MARGIN + x * TILE_SIZE + TILE_SIZE // 2,
                                      MARGIN + y * TILE_SIZE + TILE_SIZE // 2))
    screen.blit(text, text_rect)


def draw_score():
    """Draws the current score on the screen."""
    font = pygame.font.SysFont(None, 36)
    text = font.render(f"Score: {score}", True, (255, 255, 255))
    screen.blit(text, (MARGIN, 10))


def get_tile_at_pos(pos):
    """Gets the grid coordinates of a tile based on a mouse position."""
    x, y = pos
    grid_x = (x - MARGIN) // TILE_SIZE
    grid_y = (y - MARGIN) // TILE_SIZE
    if 0 <= grid_x < GRID_SIZE and 0 <= grid_y < GRID_SIZE:
        return grid_y, grid_x
    return None


def swap_tiles(pos1, pos2):
    """Swaps two tiles in the grid."""
    y1, x1 = pos1
    y2, x2 = pos2
    grid[y1][x1], grid[y2][x2] = grid[y2][x2], grid[y1][x1]


def find_matches():
    """Finds all matches of 3 or more tiles in a row or column."""
    matches = []

    # Check for horizontal matches
    for y in range(GRID_SIZE):
        match_start = 0
        for x in range(1, GRID_SIZE):
            if grid[y][x] != grid[y][match_start]:
                if x - match_start >= 3:  # Found a match of 3 or more
                    matches.append([(y, i) for i in range(match_start, x)])
                match_start = x
        # Check the last match in the row
        if GRID_SIZE - match_start >= 3:
            matches.append([(y, i) for i in range(match_start, GRID_SIZE)])

    # Check for vertical matches
    for x in range(GRID_SIZE):
        match_start = 0
        for y in range(1, GRID_SIZE):
            if grid[y][x] != grid[match_start][x]:
                if y - match_start >= 3:  # Found a match of 3 or more
                    matches.append([(i, x) for i in range(match_start, y)])
                match_start = y
        # Check the last match in the column
        if GRID_SIZE - match_start >= 3:
            matches.append([(i, x) for i in range(match_start, GRID_SIZE)])

    return matches


def create_special_tile(matches):
    """Creates special tiles when 4+ tiles are matched."""
    for match in matches:
        if len(match) == 4:  # Horizontal match
            y, x = match[0]  # First tile in the match
            grid[y][x] = HORIZONTAL_CLEARER
        elif len(match) == 5 and all(match[i][0] == match[0][0] for i in range(5)):  # Vertical match
            y, x = match[0]
            grid[y][x] = VERTICAL_CLEARER
        elif len(match) >= 5:  # Bomb tile for L/T shapes
            y, x = match[0]
            grid[y][x] = BOMB


def clear_matches(matches):
    """Clears matched tiles and updates the score."""
    global score
    for match in matches:
        score += len(match) * 10  # 10 points per tile cleared
        for y, x in match:
            if isinstance(grid[y][x], str):  # Special tiles are not cleared automatically
                continue
            grid[y][x] = None


def activate_special_tile(y, x, tile_type):
    """Activates a special tile's effect."""
    global score
    if tile_type == HORIZONTAL_CLEARER:
        score += 50  # Award points for activation
        for i in range(GRID_SIZE):  # Clear the entire row
            grid[y][i] = None
    elif tile_type == VERTICAL_CLEARER:
        score += 50  # Award points for activation
        for i in range(GRID_SIZE):  # Clear the entire column
            grid[i][x] = None
    elif tile_type == BOMB:
        score += 100  # Award points for activation
        # Clear a 3x3 area centered around the tile
        for dy in range(-1, 2):
            for dx in range(-1, 2):
                ny, nx = y + dy, x + dx
                if 0 <= ny < GRID_SIZE and 0 <= nx < GRID_SIZE:
                    grid[ny][nx] = None


def apply_gravity():
    """Applies gravity to make tiles fall into empty spaces."""
    for x in range(GRID_SIZE):
        for y in range(GRID_SIZE - 1, -1, -1):  # Start from the bottom row
            if grid[y][x] is None:  # If there's an empty space
                for above_y in range(y - 1, -1, -1):  # Look above for a tile to drop
                    if grid[above_y][x] is not None:
                        grid[y][x] = grid[above_y][x]
                        grid[above_y][x] = None
                        break
                # If no tile above, create a new one at the top
                if grid[y][x] is None:
                    grid[y][x] = random.choice(NORMAL_COLORS)


# Game loop
running = True
while running:
    # Check for matches and clear them
    matches = find_matches()
    if matches:
        create_special_tile(matches)
        clear_matches(matches)
        apply_gravity()

    # Event handling
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

        if event.type == pygame.MOUSEBUTTONDOWN:
            pos = pygame.mouse.get_pos()
            tile = get_tile_at_pos(pos)
            if tile:
                y, x = tile
                if grid[y][x] in [HORIZONTAL_CLEARER, VERTICAL_CLEARER, BOMB]:
                    # Activate special tile
                    activate_special_tile(y, x, grid[y][x])
                    grid[y][x] = None  # Remove the special tile after activation
                    apply_gravity()
                elif selected_tile:
                    # If a tile is already selected, attempt to swap
                    if abs(selected_tile[0] - y) + abs(selected_tile[1] - x) == 1:  # Adjacent tiles
                        swap_tiles(selected_tile, tile)
                        matches = find_matches()
                        if not matches:  # No match, swap back
                            swap_tiles(selected_tile, tile)
                        selected_tile = None
                    else:
                        selected_tile = tile
                else:
                    selected_tile = tile

    # Drawing
    screen.fill(BACKGROUND_COLOR)
    draw_grid()
    draw_score()  # Display the score

    # Highlight selected tile
    if selected_tile:
        y, x = selected_tile
        pygame.draw.rect(
            screen,
            HIGHLIGHT_COLOR,
            (MARGIN + x * TILE_SIZE, MARGIN + y * TILE_SIZE, TILE_SIZE, TILE_SIZE),
            3,
        )

    # Update display
    pygame.display.flip()
    clock.tick(FPS)

pygame.quit()