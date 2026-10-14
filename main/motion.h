#pragma once
// What the motion sensor (BMI270) says about how the watch is held: whether
// it lies face down, was picked up, or was shaken.

namespace motion {

// Call once per frame. Without a sensor everything stays at rest.
void update();
bool available();

bool face_down();  // has been lying face down for a moment

// Events since the last call.
bool moved();   // picked up or turned: enough to wake a dimmed screen
bool shaken();  // shaken briskly a few times

}  // namespace motion
