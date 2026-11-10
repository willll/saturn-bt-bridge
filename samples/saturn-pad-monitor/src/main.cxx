// Pad monitor: shows the buttons held on the pad in port 1, and sends every
// press and release to the USB DevCart. Read the messages on the PC with: ftx -c
// Based on the SaturnRingLib "Input - Gamepad" sample.
#include <srl.hpp>
#include <srl_log.hpp>
#include "pad_images.hpp"

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

// Loads the gamepad palette into a free color RAM bank. Returns the bank, or -1
int16_t LoadGamepadPalette(SRL::CRAM::TextureColorMode mode)
{
    // Get free CRAM bank
    int32_t id = SRL::CRAM::GetFreeBank(mode);

    if (id >= 0)
    {
        SRL::CRAM::Palette palette(mode, id);

        if (palette.Load((HighColor*)GamepadPalette, GamepadColors) >= 0)
        {
            // Mark bank as in use
            SRL::CRAM::SetBankUsedState(id, mode, true);
            return id;
        }
    }

    // No free bank found
    return -1;
}

// Load sample graphics, in the order of the identifiers above.
// They are built into the program (make_images.py): sent over the DevCart there is no CD to read files from.
void LoadGraphics()
{
    using TextureColorMode = SRL::CRAM::TextureColorMode;
    static_assert(GamepadColors > 64 && GamepadColors <= 128, "the pad picture is a 128-color image");
    const int16_t bank = LoadGamepadPalette(TextureColorMode::Paletted128);

    if (bank >= 0)
    {
        SRL::VDP1::TryLoadTexture(GamepadWidth, GamepadHeight, TextureColorMode::Paletted128, bank, (void*)GamepadPixels);
    }

    SRL::VDP1::TryLoadTexture(WireWidth, WireHeight, TextureColorMode::RGB555, 0, (void*)WirePixels);
    SRL::VDP1::TryLoadTexture(ArrowWidth, ArrowHeight, TextureColorMode::RGB555, 0, (void*)ArrowPixels);
    SRL::VDP1::TryLoadTexture(ButtonWidth, ButtonHeight, TextureColorMode::RGB555, 0, (void*)ButtonPixels);
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
