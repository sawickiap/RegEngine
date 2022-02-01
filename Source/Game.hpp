#pragma once

// Bit flags used to indicate which buttons are down during mouse even.
enum MOUSE_BUTTON_DOWN_FLAG
{
    MOUSE_BUTTON_DOWN_CONTROL = 0x1,
    MOUSE_BUTTON_DOWN_LBUTTON = 0x2,
    MOUSE_BUTTON_DOWN_MBUTTON = 0x4,
    MOUSE_BUTTON_DOWN_RBUTTON = 0x8,
    MOUSE_BUTTON_DOWN_SHIFT = 0x10,
    MOUSE_BUTTON_DOWN_XBUTTON1 = 0x20,
    MOUSE_BUTTON_DOWN_XBUTTON2 = 0x40,
};

/*
Represents main object responsible for game/program logic.
*/
class Game
{
public:
    Game();
    void Init();
    ~Game();

    // Called every frame before rendering
    void Update();

    void OnMouseMove(uint32_t buttonDownFlags, const ivec2& pos);

private:
};
