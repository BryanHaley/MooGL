/*
 * Copyright (C) The MooGL Project
 *
 * test_sfml.cpp
 * MGL
 *
 * MooGL's SFML, 3 or 2.6: the graphics module drawing through a
 * compatibility context, render textures, and a core context on request.
 */

#define GL_SILENCE_DEPRECATION
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

static int fails;

static void check(bool ok, const char *what)
{
    std::printf("  [%s] %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        fails++;
}

static bool near(std::uint8_t a, int b) { return a + 3 >= b && a <= b + 3; }

static bool pixelIs(const sf::Image &img, unsigned x, unsigned y, int r, int g, int b)
{
#if SFML_VERSION_MAJOR >= 3
    const sf::Color c = img.getPixel({x, y});
#else
    const sf::Color c = img.getPixel(x, y);
#endif
    return near(c.r, r) && near(c.g, g) && near(c.b, b);
}

static sf::Image grab(const sf::RenderWindow &window)
{
#if SFML_VERSION_MAJOR >= 3
    sf::Texture t(window.getSize());
#else
    sf::Texture t;
    t.create(window.getSize().x, window.getSize().y);
#endif
    t.update(window);
    return t.copyToImage();
}

static void graphics()
{
#if SFML_VERSION_MAJOR >= 3
    sf::RenderWindow window(sf::VideoMode({64, 64}), "MooGL SFML test", sf::Style::Titlebar);
#else
    sf::RenderWindow window(sf::VideoMode(64, 64), "MooGL SFML test", sf::Style::Titlebar);
#endif
    window.setVisible(false);

    const sf::ContextSettings s = window.getSettings();
    check(s.majorVersion == 4 && s.minorVersion == 6, "the graphics module gets a 4.6 context");
    check(!(s.attributeFlags & sf::ContextSettings::Core), "and a compatibility one, which it needs");

    const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    check(renderer && std::strncmp(renderer, "MGL", 3) == 0, "GL_RENDERER is MooGL's");

    sf::RectangleShape left(sf::Vector2f(32, 64));
    left.setFillColor(sf::Color::Green);
    window.clear(sf::Color::Red);
    window.draw(left);
    sf::Image img = grab(window);
    check(pixelIs(img, 8, 32, 0, 255, 0) && pixelIs(img, 56, 32, 255, 0, 0), "it clears and draws a shape");

    // a render texture, drawn into and then onto the window as a sprite
#if SFML_VERSION_MAJOR >= 3
    sf::RenderTexture target({16, 16});
    const bool made = target.getSize() == sf::Vector2u(16, 16);
#else
    sf::RenderTexture target;
    const bool made = target.create(16, 16);
#endif
    check(made, "a render texture");
    target.clear(sf::Color::Blue);
    target.display();

    sf::Sprite sprite(target.getTexture());
    sprite.setPosition(sf::Vector2f(32, 0));
    sprite.setScale(sf::Vector2f(2, 4));
    (void)window.setActive(true);
    window.draw(sprite);
    img = grab(window);
    check(pixelIs(img, 48, 32, 0, 0, 255) && pixelIs(img, 8, 32, 0, 255, 0), "a render texture drawn as a sprite");

    // an old-style GLSL shader, which reads the fixed-function matrices
    sf::Shader shader;
    const char *vs = "void main() { gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex; gl_FrontColor = gl_Color; }";
    const char *fs = "uniform vec4 tint; void main() { gl_FragColor = gl_Color * tint; }";
#if SFML_VERSION_MAJOR >= 3
    const bool built = shader.loadFromMemory(std::string(vs), std::string(fs));
#else
    const bool built = shader.loadFromMemory(vs, fs);
#endif
    check(built, "sf::Shader builds legacy GLSL");
    if (built)
    {
        shader.setUniform("tint", sf::Glsl::Vec4(1, 0, 1, 1));
        sf::RectangleShape all(sf::Vector2f(64, 64));
        all.setFillColor(sf::Color::White);
        window.clear(sf::Color::Black);
        window.draw(all, &shader);
        img = grab(window);
        check(pixelIs(img, 32, 32, 255, 0, 255), "and draws with it");
    }

    // text, from glyphs SFML packs into a texture
    sf::Font font;
#if SFML_VERSION_MAJOR >= 3
    const bool loaded = font.openFromFile("/System/Library/Fonts/Supplemental/Arial.ttf");
#else
    const bool loaded = font.loadFromFile("/System/Library/Fonts/Supplemental/Arial.ttf");
#endif
    check(loaded, "a font");
    if (loaded)
    {
#if SFML_VERSION_MAJOR >= 3
        sf::Text text(font, "I", 60);
#else
        sf::Text text("I", font, 60);
#endif
        const sf::FloatRect b = text.getLocalBounds();
#if SFML_VERSION_MAJOR >= 3
        text.setPosition(sf::Vector2f(16 - b.position.x - b.size.x / 2, 32 - b.position.y - b.size.y / 2));
#else
        text.setPosition(sf::Vector2f(16 - b.left - b.width / 2, 32 - b.top - b.height / 2));
#endif
        text.setFillColor(sf::Color::White);
        window.clear(sf::Color::Black);
        window.draw(text);
        img = grab(window);
        check(pixelIs(img, 16, 32, 255, 255, 255) && pixelIs(img, 2, 32, 0, 0, 0), "it draws text");
    }

    window.display();
    check(glGetError() == GL_NO_ERROR, "no GL errors");
    window.close();
}

static void coreContext()
{
    sf::ContextSettings want;
    want.majorVersion = 4;
    want.minorVersion = 1;
    want.attributeFlags = sf::ContextSettings::Core;

#if SFML_VERSION_MAJOR >= 3
    sf::Window window(sf::VideoMode({64, 64}), "MooGL SFML core test", sf::Style::Titlebar, sf::State::Windowed, want);
#else
    sf::Window window(sf::VideoMode(64, 64), "MooGL SFML core test", sf::Style::Titlebar, want);
#endif
    window.setVisible(false);

    const sf::ContextSettings s = window.getSettings();
    check(s.majorVersion == 4 && s.minorVersion == 6 && (s.attributeFlags & sf::ContextSettings::Core),
          "a 4.1 core request gets 4.6 core");

    GLint mask = 0;
    glGetIntegerv(0x9126 /* GL_CONTEXT_PROFILE_MASK */, &mask);
    check(mask == 0x1, "GL agrees it is core");
    window.display();
}

int main()
{
    graphics();
    coreContext();

    std::printf("%s\n", fails ? "FAILED" : "passed");
    return fails ? 1 : 0;
}
