#pragma once

#include <iostream>
#include <random>
#include <vector>
#include <algorithm>

using std::vector;

const vector<float> hour_activity = {
    0.5f, 0.015f, 0.05f, 0.02f, 0.01f, 0.02f,   // 00:00 - 05:00
    0.05f, 0.15f, 0.3f, 0.4f, 0.5f, 0.7f,   // 06:00 - 11:00
    0.65f, 0.5f, 0.4f, 0.5f, 0.55f, 0.60f,   // 12:00 - 17:00
    0.7f, 0.85f, 0.95f, 0.9f, 0.8f, 0.7f    // 18:00 - 23:00
};
struct Scenario {
    std::string name;
    float probability;
    float speed_mean;
    float speed_stddev;
    bool is_fraction;
};
inline const vector<Scenario> scenarios = {
    {"Download",     0.04,   0.90f,  0.05f,  true }, //Вероятность, скорость, разброс, динамичность
    {"Video 4K",     0.06,  30.0f,   4.0f,   false},
    {"Video HD",     0.22,  11.0f,   2.5f,   false},
    {"IPTV",         0.08,  13.0f,   2.0f,   false},
    {"VideoCall",    0.05,   4.0f,   0.8f,   false},
    {"Gaming",       0.06,   2.0f,   0.6f,   false},
    {"Social",       0.28,   2.5f,   1.0f,   false},
    //Остальное шум
};

inline float generate_hourly_load(int hour, int tariff_mbps, std::mt19937& rng) {
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);

    if (prob(rng) > hour_activity[hour]) return 0.0f;

    float roll = prob(rng);
    float cumulative = 0.0f;

    for (const auto& sc : scenarios) {
        cumulative += sc.probability;
        if (roll < cumulative) {
            std::normal_distribution<float> dist(sc.speed_mean, sc.speed_stddev);
            float raw = dist(rng);

            float speed;
            if (sc.is_fraction) {
                speed = raw * tariff_mbps;
            }
            else {
                speed = raw;
            }

            // Ограничения
            speed = std::max(0.0f, speed);
            speed = std::min(speed, (float)tariff_mbps);
            return speed;
        }
    }

    //Фоновый шум
    std::normal_distribution<float> bg(1.0f, 0.5f);
    return std::min(std::max(0.0f, bg(rng)), (float)tariff_mbps);
}