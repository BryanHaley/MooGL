/*
 * Copyright (C) The MooGL Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SFML 3's graphics module on MooGL: a spinning square and a circle, drawn
 * through a compatibility context. --frames N closes after N frames.
 */

#include <SFML/Graphics.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>

int main(int argc, char **argv)
{
    const int frames = argc > 2 && !std::strcmp(argv[1], "--frames") ? std::atoi(argv[2]) : 0;

    sf::RenderWindow window(sf::VideoMode({640, 480}), "MooGL SFML shapes");
    window.setVerticalSyncEnabled(true);

    const sf::ContextSettings settings = window.getSettings();
    std::cout << "OpenGL " << settings.majorVersion << "." << settings.minorVersion << "\n";

    sf::RectangleShape square({160, 160});
    square.setOrigin({80, 80});
    square.setPosition({220, 240});
    square.setFillColor(sf::Color(230, 120, 60));

    sf::CircleShape circle(80);
    circle.setOrigin({80, 80});
    circle.setPosition({440, 240});
    circle.setFillColor(sf::Color(60, 160, 230));

    sf::Clock clock;
    for (int frame = 0; window.isOpen() && (!frames || frame < frames); frame++)
    {
        while (const std::optional event = window.pollEvent())
            if (event->is<sf::Event::Closed>())
                window.close();

        square.setRotation(sf::degrees(clock.getElapsedTime().asSeconds() * 90));

        window.clear(sf::Color(20, 22, 30));
        window.draw(square);
        window.draw(circle);
        window.display();
    }
    return 0;
}
