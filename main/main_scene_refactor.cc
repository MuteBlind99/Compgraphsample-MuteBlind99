#include <iostream>
#include <__msvc_ostream.hpp>

#include "engine/engine.h"
#include "engine/window.h"
#include "Refactor/final_scene.h"
//
// Created by forna on 10.02.2026.
//
std::unique_ptr<FinalScene> scene_final;

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "Shadow/SSAO DEMO - Techniques Avancé" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Contrôles:" << std::endl;
        std::cout << "  TAB: Toggle camera control" << std::endl;
        std::cout << "  0-9: Changer effet de post-processing" << std::endl;
        std::cout << "  ESPACE: Toggle bloom effect" << std::endl;
        std::cout << "  F1: Deferred Shading" << std::endl;
        std::cout << "  F2: Shadow Mapping" << std::endl;
        std::cout << "  F3: SSAO (Screen Space Ambient Occlusion)" << std::endl;
        std::cout << "  F4: Debug Shadow Map" << std::endl;
        std::cout << "  F5: Debug SSAO Buffer" << std::endl;
        std::cout << "========================================" << std::endl;

        common::WindowConfig config;
        config.width = 1280;
        config.height = 720;
        config.title = "Shadow/SSAO Demo - Techniques Avancé";
        config.renderer = common::WindowConfig::RendererType::OPENGL;
        config.fixed_dt = 1.0f / 60.0f;
        common::SetWindowConfig(config);

        scene_final = std::make_unique<FinalScene>();
        common::SystemObserverSubject::AddObserver(scene_final.get());
        common::DrawObserverSubject::AddObserver(scene_final.get());

        common::RunEngine();

        common::SystemObserverSubject::RemoveObserver(scene_final.get());
        common::DrawObserverSubject::RemoveObserver(scene_final.get());
        scene_final.reset();

        std::cout << "\n✓ Démonstration terminée avec succès!" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "\n✗ ERREUR: " << e.what() << std::endl;

        if (scene_final) {
            common::SystemObserverSubject::RemoveObserver(scene_final.get());
            common::DrawObserverSubject::RemoveObserver(scene_final.get());
            scene_final.reset();
        }
        return -1;
    }
    return 0;
}