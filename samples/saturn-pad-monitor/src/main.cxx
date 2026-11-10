// Pad monitor: shows the buttons held on the pad in port 1, and sends every
// press and release to the USB DevCart. Read the messages on the PC with: ftx -c
// Based on the SaturnRingLib "Input - Gamepad" sample.
#include <srl.hpp>
#include <srl_log.hpp>

// Using to shorten names for Vector and HighColor
using namespace SRL::Types;
using namespace SRL::Math::Types;

// Using to shorten names for input and logs
using namespace SRL::Input;
using namespace SRL::Logger;

// Graphics identifiers, in LoadGraphics() order
#define BackgroundPad (0)
#define BackgroundWire (1)
#define InputArrow (2)
#define InputButton (3)

using SRL::Math::Angle;

// Names match the saturn-bt-bridge commands, so a log line reads like the command that caused it.
// Sprite positions and angles come from the "Input - Gamepad" sample.
struct PadButton
{
    Digital::Button button;
    const char* name;
    int sprite;
    Vector3D position;
    Angle angle;
};

static const PadButton Buttons[] = {
    {Digital::Button::Up, "UP", InputArrow, Vector3D(-65, -17, 500), Angle::FromDegrees(90)},
    {Digital::Button::Down, "DOWN", InputArrow, Vector3D(-65, 17, 500), Angle::FromDegrees(-90)},
    {Digital::Button::Left, "LEFT", InputArrow, Vector3D(-85, 0, 500), Angle::FromDegrees(0)},
    {Digital::Button::Right, "RIGHT", InputArrow, Vector3D(-50, 0, 500), Angle::FromDegrees(180)},
    {Digital::Button::START, "START", InputButton, Vector3D(0, 20, 500), Angle::FromDegrees(0)},
    {Digital::Button::L, "L", InputArrow, Vector3D(-90, -70, 500), Angle::FromDegrees(-135)},
    {Digital::Button::R, "R", InputArrow, Vector3D(90, -70, 500), Angle::FromDegrees(-45)},
    {Digital::Button::A, "A", InputButton, Vector3D(45, 22, 500), Angle::FromDegrees(0)},
    {Digital::Button::B, "B", InputButton, Vector3D(69, 5, 500), Angle::FromDegrees(0)},
    {Digital::Button::C, "C", InputButton, Vector3D(98, -5, 500), Angle::FromDegrees(0)},
    {Digital::Button::X, "X", InputButton, Vector3D(37, -7, 500), Angle::FromDegrees(0)},
    {Digital::Button::Y, "Y", InputButton, Vector3D(57, -20, 500), Angle::FromDegrees(0)},
    {Digital::Button::Z, "Z", InputButton, Vector3D(80, -30, 500), Angle::FromDegrees(0)},
};

// Palette for background gamepad
int16_t LoadGamepadPalette(SRL::Bitmap::BitmapInfo* bitmap)
{
    // Get free CRAM bank
    int32_t id = SRL::CRAM::GetFreeBank(bitmap->ColorMode);

    if (id >= 0)
    {
        SRL::CRAM::Palette palette(bitmap->ColorMode, id);

        if (palette.Load((HighColor*)bitmap->Palette->Colors, bitmap->Palette->Count) >= 0)
        {
            // Mark bank as in use
            SRL::CRAM::SetBankUsedState(id, bitmap->ColorMode, true);
            return id;
        }
    }

    // No free bank found
    return -1;
}

// Load sample graphics
void LoadGraphics()
{
    SRL::Bitmap::TGA* background = new SRL::Bitmap::TGA("GAMEPAD.TGA");
    SRL::VDP1::TryLoadTexture(background, LoadGamepadPalette);
    delete background;

    SRL::Bitmap::TGA* wire = new SRL::Bitmap::TGA("WIRE.TGA");
    SRL::VDP1::TryLoadTexture(wire);
    delete wire;

    SRL::Bitmap::TGA* arrow = new SRL::Bitmap::TGA("ARROW.TGA");
    SRL::VDP1::TryLoadTexture(arrow);
    delete arrow;

    SRL::Bitmap::TGA* button = new SRL::Bitmap::TGA("BUTTON.TGA");
    SRL::VDP1::TryLoadTexture(button);
    delete button;
}

// Main program entry
int main()
{
    using namespace SRL::Math;

    // Initialize library
    SRL::Core::Initialize(HighColor::Colors::Black);
    SRL::Debug::Print(1, 1, "Pad monitor");

    // Load graphics
    LoadGraphics();

    // Without a DevCart (e.g. in an emulator) don't log: writes would wait forever on a missing USB FIFO
    const bool devCart = SRL::DevCart::CS0::IsConnected();
    SRL::Debug::Print(1, 24, devCart ? "DevCart: found, read with ftx -c" : "DevCart: not found");

    if (devCart)
    {
        LogInfo("PAD MONITOR STARTED");
    }

    // Initialize gamepad on port 0
    Digital port0(0);
    bool wasConnected = false;

    // Main program loop
    while (1)
    {
        // Draw background
        SRL::Scene2D::DrawSprite(BackgroundPad, Vector3D(0, 0, 500));
        SRL::Scene2D::DrawSprite(BackgroundWire, Vector3D(0, -100, 500), Vector2D(1, 40));

        // Report the pad (or Bluetooth receiver) being plugged in or out
        const bool connected = port0.IsConnected();

        if (connected != wasConnected)
        {
            wasConnected = connected;

            if (devCart)
            {
                LogInfo(connected ? "PAD CONNECTED" : "PAD DISCONNECTED");
            }
        }

        // Names of held buttons; all of them together fit in 64 characters
        char held[64] = "";
        char* end = held;

        if (connected)
        {
            for (const PadButton& b : Buttons)
            {
                if (port0.IsHeld(b.button))
                {
                    SRL::Scene2D::DrawSprite(b.sprite, b.position, b.angle);

                    if (end != held)
                    {
                        *end++ = ' ';
                    }

                    for (const char* c = b.name; *c != '\0'; c++)
                    {
                        *end++ = *c;
                    }

                    *end = '\0';
                }

                if (devCart && port0.WasPressed(b.button))
                {
                    LogInfo("PRESSED %s", b.name);
                }

                if (devCart && port0.WasReleased(b.button))
                {
                    LogInfo("RELEASED %s", b.name);
                }
            }
        }

        // Held buttons as text, under the pad picture
        SRL::Debug::Print(1, 25, "Held:");
        SRL::Debug::PrintClearLine(26);
        SRL::Debug::Print(1, 26, connected ? held : "no pad in port 1");

        // Refresh screen
        SRL::Core::Synchronize();
    }

    return 0;
}
