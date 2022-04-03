#pragma once

enum class MouseButton { Left, Middle, Right, Count };

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

enum KEY_MODIFIERS
{
    KEY_MODIFIER_CONTROL = 0x1,
    KEY_MODIFIER_ALT = 0x2,
    KEY_MODIFIER_SHIFT = 0x4,
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

    void OnKeyDown(WPARAM key, uint32_t modifiers);
    void OnKeyUp(WPARAM key, uint32_t modifiers);
    void OnChar(wchar_t ch);
    void OnMouseDown(MouseButton button, uint32_t buttonDownFlags, const ivec2& pos);
    void OnMouseUp(MouseButton button, uint32_t buttonDownFlags, const ivec2& pos);
    void OnMouseMove(uint32_t buttonDownFlags, const ivec2& pos);
    void OnMouseWheel(int16_t distance, uint32_t buttonDownFlags, const ivec2& pos);

private:
    bool m_DemoWindowVisible = false;
    bool m_MetricsWindowVisible = false;
    bool m_StackToolWindowVisible = false;
    bool m_AboutWindowVisible = false;

    void ImGui();
    void ShowAboutWindow();
};
