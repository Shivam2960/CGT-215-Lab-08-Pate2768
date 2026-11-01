#include <iostream>           // Lets us print messages to the console.
#include <SFML/Graphics.hpp>  // Provides the window, textures, text, drawing, and timing.
#include <SFPhysics.h>        // Provides physics objects and collision handling.
#include <vector>
#include <SFML/Audio.hpp>     // Provides sound effects and background music.

using namespace std;
using namespace sf;
using namespace sfp;

// Return the displayed width and height of a text object.
// The width helps us position the score near the right edge.
Vector2f GetTextSize(Text text) {
    FloatRect r = text.getGlobalBounds();
    return Vector2f(r.width, r.height);
}

int main()
{
    // Create an 800-by-600 window with "Balloon Hunter" in its title bar.
    RenderWindow window(VideoMode(800, 600), "Balloon Hunter");

    // Limit drawing to approximately 60 frames per second.
    window.setFramerateLimit(60);

    // Holding Space should not create repeated key-press events.
    window.setKeyRepeatEnabled(false);

    // Create a physics world without gravity.
    // Objects move according to the velocities we give them.
    World world(Vector2f(0, 0));

    // Start with zero points and five available shots.
    int score = 0;
    int arrows = 5;

    // Track whether an arrow is flying and whether gameplay has ended.
    bool drawingArrow = false;
    bool gameOver = false;

    // Create textures to hold the crossbow, arrow, and balloon images.
    Texture cbowTex;
    Texture arrowTex;
    Texture redTex;

    // Load the images from the images folder.
    // Stop the program if a required image cannot be loaded.
    if (!cbowTex.loadFromFile("images/crossbow.png") ||
        !arrowTex.loadFromFile("images/arrow.png") ||
        !redTex.loadFromFile("images/red_balloon.png")) {
        cout << "Could not load an image." << endl;
        return 1;
    }

    // Load the font used for the score, arrow count, and game-over message.
    Font fnt;
    if (!fnt.loadFromFile("arial.ttf")) {
        cout << "Could not load font." << endl;
        return 1;
    }

    // Load the pop sound's audio data into a buffer.
    SoundBuffer popBuffer;
    if (!popBuffer.loadFromFile("balloonpop.ogg")) {
        cout << "Could not load balloonpop.ogg" << endl;
        return 1;
    }

    // Connect the sound player to the loaded audio buffer.
    Sound popSound;
    popSound.setBuffer(popBuffer);

    // Open the background music file.
    Music music;
    if (!music.openFromFile("circus.ogg")) {
        cout << "Could not load circus.ogg" << endl;
        return 1;
    }

    // Repeat the music when it reaches the end and begin playback.
    music.setLoop(true);
    music.play();

    // Create the crossbow and give it its image.
    PhysicsSprite crossBow;
    crossBow.setTexture(cbowTex);

    // Center the crossbow horizontally.
    // Subtract half its height so its bottom edge lines up with the window bottom.
    Vector2f bowSize = crossBow.getSize();
    crossBow.setCenter(Vector2f(400, 600 - bowSize.y / 2));

    // Create one arrow that will be reused for each shot.
    // It is added to the physics world only when the player fires.
    PhysicsSprite arrow;
    arrow.setTexture(arrowTex);

    // Create a stationary boundary across the top of the window.
    // This catches arrows that miss the balloons.
    PhysicsRectangle top;
    top.setSize(Vector2f(800, 10));
    top.setCenter(Vector2f(400, 5));
    top.setStatic(true);
    world.AddPhysicsBody(top);

    // Register a lambda to run when the top boundary collides with an object.
    // Reference captures let it access the original arrow, world, and flag.
    // Assigning this callback does not run it immediately.
    top.onCollision = [&drawingArrow, &world, &arrow]
    (PhysicsBodyCollisionResult result) {
        // Only finish the shot if the other object is the active arrow.
        if (drawingArrow && result.object2 == arrow) {
            drawingArrow = false;
            world.RemovePhysicsBody(arrow);
        }
        };

    // Create a stationary boundary beyond the right edge.
    // Its position lets balloons leave the screen completely before removal.
    PhysicsRectangle right;
    right.setSize(Vector2f(10, 600));
    right.setCenter(Vector2f(
        810.0f + static_cast<float>(redTex.getSize().x), 300));
    right.setStatic(true);
    world.AddPhysicsBody(right);

    // Store the balloons in a list that supports creation and queued removal.
    // There is no left boundary because balloons enter from that side.
    PhysicsShapeList<PhysicsSprite> balloons;

    // Space balloon creation one second apart.
    // Starting at zero allows the first balloon to appear immediately.
    const int SPAWN_INTERVAL_MS = 1000;
    int spawnCountdown = 0;

    // Define a lambda that creates one balloon whenever we call createBalloon().
    // [&] captures the surrounding variables it needs by reference.
    auto createBalloon = [&]() {
        // Create a balloon in the list and access it through a reference.
        PhysicsSprite& balloon = balloons.Create();
        balloon.setTexture(redTex);

        // Start the balloon completely offscreen to the left.
        // Place its top edge 20 pixels below the top of the window.
        Vector2f size = balloon.getSize();
        balloon.setCenter(Vector2f(
            -size.x / 2 - 10,
            20 + size.y / 2));

        // Move right at 0.25 pixels per millisecond without vertical movement.
        // Register the balloon so the world can move it and detect collisions.
        balloon.setVelocity(Vector2f(0.25f, 0));
        world.AddPhysicsBody(balloon);

        // Store the address of this particular balloon.
        PhysicsSprite* balloonPtr = &balloon;

        // Give this balloon its own collision callback.
        // [&] accesses shared game variables by reference.
        // balloonPtr is copied so the callback keeps this balloon's address
        // after the local pointer variable goes out of scope.
        // Each callback starts with its own removed flag set to false.
        // mutable allows the callback to change that captured flag.
        balloon.onCollision =
            [&, balloonPtr, removed = false]
            (PhysicsBodyCollisionResult result) mutable {

            // Ignore further collisions if this balloon is already queued for removal.
            // return exits this callback, not the main function.
            if (removed) {
                return;
            }

            // A hit from the active arrow earns ten points and plays the pop sound.
            if (drawingArrow && result.object2 == arrow) {
                removed = true;
                popSound.play();
                score += 10;

                // Finish the shot and request removal of both physics objects.
                // Dereferencing balloonPtr gives us the balloon it points to.
                drawingArrow = false;
                world.RemovePhysicsBody(arrow);
                world.RemovePhysicsBody(*balloonPtr);

                // Queue the balloon for deletion after the physics update.
                balloons.QueueRemove(*balloonPtr);
            }
            else if (result.object2 == right) {
                // Remove balloons that reach the exit boundary without awarding points.
                removed = true;
                world.RemovePhysicsBody(*balloonPtr);
                balloons.QueueRemove(*balloonPtr);
            }
            };
        };

    // Set the font and character size for the score.
    Text scoreText;
    scoreText.setFont(fnt);
    scoreText.setCharacterSize(24);

    // Use matching text settings for the remaining arrow count.
    Text arrowCountText;
    arrowCountText.setFont(fnt);
    arrowCountText.setCharacterSize(24);

    // Create a larger message to display when gameplay ends.
    Text gameOverText;
    gameOverText.setFont(fnt);
    gameOverText.setCharacterSize(48);
    gameOverText.setString("GAME OVER");

    // Move the text's origin to the center of its bounds.
    // Positioning that origin at (400, 300) centers the message in the window.
    FloatRect bounds = gameOverText.getLocalBounds();
    gameOverText.setOrigin(
        bounds.left + bounds.width / 2,
        bounds.top + bounds.height / 2);
    gameOverText.setPosition(400, 300);

    // Start a clock to measure the time between loop iterations.
    Clock clock;

    // Repeat while the window is open.
    while (window.isOpen()) {
        // Read the elapsed time in milliseconds and restart the clock.
        int elapsedMS =
            static_cast<int>(clock.restart().asMilliseconds());

        // Limit the time simulated in one update.
        // This prevents large movement jumps after a pause or window drag.
        if (elapsedMS > 50) {
            elapsedMS = 50;
        }

        // Start each frame without a pending shot request.
        bool fireRequested = false;

        // Process pending window events, such as closing the window or pressing Space.
        Event event;
        while (window.pollEvent(event)) {
            if (event.type == Event::Closed) {
                window.close();
            }

            // Respond to a new press of the Space key.
            if (event.type == Event::KeyPressed &&
                event.key.code == Keyboard::Space) {

                // Pressing Space after game over closes the window.
                if (gameOver) {
                    window.close();
                }
                else if (arrows > 0 && !drawingArrow) {
                    // Request a shot only if ammunition remains
                    // and the previous arrow has finished flying.
                    fireRequested = true;
                }
            }
        }

        // Stop the main loop immediately if an event closed the window.
        if (!window.isOpen()) {
            break;
        }

        // Update gameplay only while the game is active.
        if (!gameOver) {
            if (fireRequested) {
                // Mark the arrow as active and use one of the remaining shots.
                drawingArrow = true;
                arrows--;

                // Start the arrow at the crossbow and send it upward.
                // Negative Y moves toward the top of the window.
                arrow.setCenter(crossBow.getCenter());
                arrow.setVelocity(Vector2f(0, -1));
                world.AddPhysicsBody(arrow);
            }

            // Count down the time remaining until the next balloon.
            spawnCountdown -= elapsedMS;

            if (spawnCountdown <= 0) {
                // Run the creation lambda and add another second to the timer.
                createBalloon();
                spawnCountdown += SPAWN_INTERVAL_MS;
            }

            // Update movement and collisions only when positive time has passed.
            // A zero-time update can cause this physics library to freeze.
            if (elapsedMS > 0) {
                world.UpdatePhysics(elapsedMS);
            }

            // Delete balloons queued by the collision callbacks.
            balloons.DoRemovals();

            // End gameplay after all five shots have been used
            // and the final arrow has hit a balloon or the top boundary.
            if (arrows == 0 && !drawingArrow) {
                gameOver = true;
            }
        }

        // Erase the previous frame with a black background.
        // Drawing continues after game over, while gameplay updates stay paused.
        window.clear();

        // Draw each balloon that remains in the list.
        for (PhysicsSprite& balloon : balloons) {
            window.draw(balloon);
        }

        // Draw the stationary crossbow.
        window.draw(crossBow);

        // Draw the arrow only while it is flying.
        if (drawingArrow) {
            window.draw(arrow);
        }

        // Update and draw the remaining shot count near the bottom-left corner.
        arrowCountText.setString("Arrows: " + to_string(arrows));
        arrowCountText.setPosition(10, 560);
        window.draw(arrowCountText);

        // Update the score and use its width to position it near the bottom-right corner.
        scoreText.setString("Score: " + to_string(score));
        scoreText.setPosition(
            790 - GetTextSize(scoreText).x, 560);
        window.draw(scoreText);

        // Draw the game-over message over the frozen scene.
        if (gameOver) {
            window.draw(gameOverText);
        }

        // Show the completed frame.
        window.display();
    }

    // Request removal of any balloons left when the window closes.
    // Queue them first so we do not delete objects while iterating over the list.
    for (PhysicsSprite& balloon : balloons) {
        world.RemovePhysicsBody(balloon);
        balloons.QueueRemove(balloon);
    }

    // Delete the remaining dynamically created balloons.
    balloons.DoRemovals();

    // Report that the program finished successfully.
    return 0;
}