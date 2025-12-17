//
// Created by forna on 16.12.2025.
//

// input.h
#ifndef INPUT_H
#define INPUT_H

#include <unordered_map>
#include <vector>
#include <array>
#include <iostream>

namespace common {
    // ==================== ENUMS POUR LES TOUCHES ET SOURIS ====================
    enum class KeyCode {
        // Lettres
        A = 'A', B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        // Chiffres
        NUM0 = '0', NUM1, NUM2, NUM3, NUM4,
        NUM5, NUM6, NUM7, NUM8, NUM9,

        // Fonctions
        F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

        // Touches spéciales
        SPACE = 32,
        ESCAPE = 256,
        ENTER = 257,
        TAB = 258,
        BACKSPACE = 259,
        INSERT = 260,
        DELETE = 261,
        RIGHT = 262,
        LEFT = 263,
        DOWN = 264,
        UP = 265,
        PAGE_UP = 266,
        PAGE_DOWN = 267,
        HOME = 268,
        END = 269,
        CAPS_LOCK = 280,
        SCROLL_LOCK = 281,
        NUM_LOCK = 282,
        PRINT_SCREEN = 283,
        PAUSE = 284,

        // Modificateurs
        LEFT_SHIFT = 340,
        LEFT_CONTROL = 341,
        LEFT_ALT = 342,
        LEFT_SUPER = 343,
        RIGHT_SHIFT = 344,
        RIGHT_CONTROL = 345,
        RIGHT_ALT = 346,
        RIGHT_SUPER = 347,
        MENU = 348,

        // Pavé numérique
        KP_0 = 320, KP_1, KP_2, KP_3, KP_4,
        KP_5, KP_6, KP_7, KP_8, KP_9,
        KP_DECIMAL = 330,
        KP_DIVIDE = 331,
        KP_MULTIPLY = 332,
        KP_SUBTRACT = 333,
        KP_ADD = 334,
        KP_ENTER = 335,
        KP_EQUAL = 336,

        // Autres
        GRAVE_ACCENT = 96,
        MINUS = 45,
        EQUAL = 61,
        LEFT_BRACKET = 91,
        RIGHT_BRACKET = 93,
        BACKSLASH = 92,
        SEMICOLON = 59,
        APOSTROPHE = 39,
        COMMA = 44,
        PERIOD = 46,
        SLASH = 47,

        // Pour compatibilité
        PLUS = 334, // KP_ADD
    };

    enum class MouseButton {
        LEFT = 0,
        RIGHT = 1,
        MIDDLE = 2,
        BUTTON_4 = 3,
        BUTTON_5 = 4,
        BUTTON_6 = 5,
        BUTTON_7 = 6,
        BUTTON_8 = 7,

        COUNT = 8
    };

    enum class InputAction {
        RELEASE = 0,
        PRESS = 1,
        REPEAT = 2
    };

    // ==================== STRUCTURES POUR LES ÉVÉNEMENTS ====================
    struct KeyEvent {
        KeyCode key;
        InputAction action;
        int scancode;
        int mods;

        KeyEvent(KeyCode k, InputAction a, int s, int m)
            : key(k), action(a), scancode(s), mods(m) {
        }
    };

    struct MouseButtonEvent {
        MouseButton button;
        InputAction action;
        int mods;
        float x;
        float y;

        MouseButtonEvent(MouseButton b, InputAction a, int m, float xPos, float yPos)
            : button(b), action(a), mods(m), x(xPos), y(yPos) {
        }
    };

    struct MouseMoveEvent {
        float x;
        float y;
        float deltaX;  // xrel en SDL3
        float deltaY;  // yrel en SDL3

        MouseMoveEvent(float xPos, float yPos, float dX, float dY)
            : x(xPos), y(yPos), deltaX(dX), deltaY(dY) {}
    };

    struct MouseScrollEvent {
        float offsetX;
        float offsetY;

        MouseScrollEvent(float x, float y) : offsetX(x), offsetY(y) {
        }
    };

    // ==================== INTERFACE D'ABONNEMENT ====================
    class InputSubscriber {
    public:
        virtual ~InputSubscriber() = default;

        virtual void OnKeyEvent(const KeyEvent &event) {
        }

        virtual void OnMouseButtonEvent(const MouseButtonEvent &event) {
        }

        virtual void OnMouseMoveEvent(const MouseMoveEvent &event) {
        }

        virtual void OnMouseScrollEvent(const MouseScrollEvent &event) {
        }

        virtual void OnInputFrameBegin() {
        }

        virtual void OnInputFrameEnd() {
        }
    };

    // ==================== CLASS PRINCIPALE INPUT ====================
    class Input {
    private:
        Input() = default;

        ~Input() = default;

    public:
        // Supprimer copie et déplacement
        Input(const Input &) = delete;

        Input &operator=(const Input &) = delete;

        Input(Input &&) = delete;

        Input &operator=(Input &&) = delete;

        // Singleton
        static Input &GetInstance() {
            static Input instance;
            return instance;
        }

        // ==================== INITIALISATION ====================
        void Initialize() {
            // Réinitialiser tous les états
            keyStates_.clear();
            mouseButtonStates_.clear();
            mouseX_ = 0.0f;
            mouseY_ = 0.0f;
            mouseDeltaX_ = 0.0f;
            mouseDeltaY_ = 0.0f;
            scrollX_ = 0.0f;
            scrollY_ = 0.0f;
            lastMouseX_ = 0.0f;
            lastMouseY_ = 0.0f;
            firstMouse_ = true;

            std::cout << "✓ Système d'entrée initialisé" << std::endl;
        }

        void Shutdown() {
            subscribers_.clear();
            keyStates_.clear();
            mouseButtonStates_.clear();
            std::cout << "✗ Système d'entrée arrêté" << std::endl;
        }

        // ==================== GESTION DES ABONNÉS ====================
        void Subscribe(InputSubscriber *subscriber) {
            if (subscriber && std::find(subscribers_.begin(), subscribers_.end(), subscriber) == subscribers_.end()) {
                subscribers_.push_back(subscriber);
            }
        }

        void Unsubscribe(InputSubscriber *subscriber) {
            auto it = std::find(subscribers_.begin(), subscribers_.end(), subscriber);
            if (it != subscribers_.end()) {
                subscribers_.erase(it);
            }
        }

        // ==================== MÉTHODES DE MISE À JOUR ====================
        void BeginFrame() {
            // Notifier les abonnés du début de frame
            for (auto *sub: subscribers_) {
                sub->OnInputFrameBegin();
            }

            // Réinitialiser les valeurs delta pour la frame
            mouseDeltaX_ = 0.0f;
            mouseDeltaY_ = 0.0f;
            scrollX_ = 0.0f;
            scrollY_ = 0.0f;
        }

        void EndFrame() {
            // Mettre à jour la dernière position de la souris
            lastMouseX_ = mouseX_;
            lastMouseY_ = mouseY_;

            // Notifier les abonnés de la fin de frame
            for (auto *sub: subscribers_) {
                sub->OnInputFrameEnd();
            }
        }

        // ==================== GESTION DES ÉVÉNEMENTS ====================
        void ProcessKeyEvent(KeyCode key, InputAction action, int scancode, int mods) {
            // Mettre à jour l'état de la touche
            keyStates_[key] = (action != InputAction::RELEASE);

            // Créer et propager l'événement
            KeyEvent event(key, action, scancode, mods);
            for (auto *sub: subscribers_) {
                sub->OnKeyEvent(event);
            }
        }

        void ProcessMouseButtonEvent(MouseButton button, InputAction action, int mods) {
            // Mettre à jour l'état du bouton
            mouseButtonStates_[button] = (action != InputAction::RELEASE);

            // Créer et propager l'événement
            MouseButtonEvent event(button, action, mods, mouseX_, mouseY_);
            for (auto *sub: subscribers_) {
                sub->OnMouseButtonEvent(event);
            }
        }

        void ProcessMouseMoveEvent(float x, float y, float deltaX = 0.0f, float deltaY = 0.0f) {
            if (firstMouse_) {
                lastMouseX_ = x;
                lastMouseY_ = y;
                firstMouse_ = false;
                return;
            }

            // Utiliser les deltas fournis par SDL3 (xrel, yrel)
            if (deltaX != 0.0f || deltaY != 0.0f) {
                mouseDeltaX_ = deltaX;
                mouseDeltaY_ = deltaY;
            } else {
                // Fallback: calculer les deltas nous-mêmes
                mouseDeltaX_ = x - lastMouseX_;
                mouseDeltaY_ = lastMouseY_ - y; // Inversé
            }

            // Mettre à jour la position
            mouseX_ = x;
            mouseY_ = y;

            // Créer et propager l'événement
            MouseMoveEvent event(x, y, mouseDeltaX_, mouseDeltaY_);
            for (auto* sub : subscribers_) {
                sub->OnMouseMoveEvent(event);
            }
        }

        void ProcessMouseScrollEvent(float offsetX, float offsetY) {
            scrollX_ = offsetX;
            scrollY_ = offsetY;

            // Créer et propager l'événement
            MouseScrollEvent event(offsetX, offsetY);
            for (auto *sub: subscribers_) {
                sub->OnMouseScrollEvent(event);
            }
        }

        // ==================== REQUÊTES D'ÉTAT ====================
        [[nodiscard]] bool IsKeyPressed(KeyCode key) const {
            auto it = keyStates_.find(key);
            return it != keyStates_.end() && it->second;
        }

        [[nodiscard]] bool IsKeyReleased(KeyCode key) const {
            return !IsKeyPressed(key);
        }

        [[nodiscard]] bool IsKeyDown(KeyCode key) const {
            // Pour la détection d'appui unique, on a besoin d'un système de frame
            // Pour l'instant, on utilise juste l'état pressé
            return IsKeyPressed(key);
        }

        [[nodiscard]] bool IsMouseButtonPressed(MouseButton button) const {
            auto it = mouseButtonStates_.find(button);
            return it != mouseButtonStates_.end() && it->second;
        }

        [[nodiscard]] bool IsMouseButtonReleased(MouseButton button) const {
            return !IsMouseButtonPressed(button);
        }

        bool GetMousePosition(float &x, float &y) const {
            x = mouseX_;
            y = mouseY_;
            return true;
        }

        bool GetMouseDelta(float &deltaX, float &deltaY) const {
            deltaX = mouseDeltaX_;
            deltaY = mouseDeltaY_;
            return true;
        }

        bool GetMouseScroll(float &scrollX, float &scrollY) const {
            scrollX = scrollX_;
            scrollY = scrollY_;
            return true;
        }

        [[nodiscard]] float GetMouseX() const { return mouseX_; }
        [[nodiscard]] float GetMouseY() const { return mouseY_; }
        [[nodiscard]] float GetMouseDeltaX() const { return mouseDeltaX_; }
        [[nodiscard]] float GetMouseDeltaY() const { return mouseDeltaY_; }
        [[nodiscard]] float GetScrollX() const { return scrollX_; }
        [[nodiscard]] float GetScrollY() const { return scrollY_; }

        // ==================== FONCTIONS UTILITAIRES ====================
        void SetMousePosition(float x, float y) {
            lastMouseX_ = mouseX_;
            lastMouseY_ = mouseY_;
            mouseX_ = x;
            mouseY_ = y;
        }

        void SetFirstMouse(bool first) {
            firstMouse_ = first;
        }

        void PrintInputState() const {
            std::cout << "\n=== ÉTAT DES ENTRÉES ===" << std::endl;
            std::cout << "Souris: (" << mouseX_ << ", " << mouseY_ << ")" << std::endl;
            std::cout << "Delta: (" << mouseDeltaX_ << ", " << mouseDeltaY_ << ")" << std::endl;
            std::cout << "Scroll: (" << scrollX_ << ", " << scrollY_ << ")" << std::endl;

            std::cout << "Touches pressées: ";
            for (const auto &[key, pressed]: keyStates_) {
                if (pressed) {
                    std::cout << static_cast<char>(static_cast<int>(key)) << " ";
                }
            }
            std::cout << std::endl;

            std::cout << "Boutons souris pressés: ";
            for (const auto &[button, pressed]: mouseButtonStates_) {
                if (pressed) {
                    std::cout << static_cast<int>(button) << " ";
                }
            }
            std::cout << std::endl;
            std::cout << "=====================" << std::endl;
        }

    private:
        // États des touches et boutons
        std::unordered_map<KeyCode, bool> keyStates_;
        std::unordered_map<MouseButton, bool> mouseButtonStates_;

        // État de la souris
        float mouseX_ = 0.0f;
        float mouseY_ = 0.0f;
        float lastMouseX_ = 0.0f;
        float lastMouseY_ = 0.0f;
        float mouseDeltaX_ = 0.0f;
        float mouseDeltaY_ = 0.0f;
        float scrollX_ = 0.0f;
        float scrollY_ = 0.0f;
        bool firstMouse_ = true;

        // Abonnés aux événements
        std::vector<InputSubscriber *> subscribers_;
    };

    // ==================== WRAPPER SIMPLIFIÉ POUR LE MOTEUR ====================
    class InputManager {
    public:
        static void Initialize() {
            Input::GetInstance().Initialize();
        }

        static void Shutdown() {
            Input::GetInstance().Shutdown();
        }

        static void BeginFrame() {
            Input::GetInstance().BeginFrame();
        }

        static void EndFrame() {
            Input::GetInstance().EndFrame();
        }

        static bool IsKeyPressed(KeyCode key) {
            return Input::GetInstance().IsKeyPressed(key);
        }

        static bool IsKeyReleased(KeyCode key) {
            return Input::GetInstance().IsKeyReleased(key);
        }

        static bool IsKeyDown(KeyCode key) {
            return Input::GetInstance().IsKeyDown(key);
        }

        static bool IsMouseButtonPressed(MouseButton button) {
            return Input::GetInstance().IsMouseButtonPressed(button);
        }

        static bool IsMouseButtonReleased(MouseButton button) {
            return Input::GetInstance().IsMouseButtonReleased(button);
        }

        static void GetMousePosition(float &x, float &y) {
            Input::GetInstance().GetMousePosition(x, y);
        }

        static float GetMouseX() {
            return Input::GetInstance().GetMouseX();
        }

        static float GetMouseY() {
            return Input::GetInstance().GetMouseY();
        }

        static void GetMouseDelta(float &deltaX, float &deltaY) {
            Input::GetInstance().GetMouseDelta(deltaX, deltaY);
        }

        static void GetMouseScroll(float &scrollX, float &scrollY) {
            Input::GetInstance().GetMouseScroll(scrollX, scrollY);
        }

        // Méthodes pour intégration avec GLFW (optionnel)
        static int ConvertGLFWKeyToKeyCode(int glfwKey) {
            // Mapping simple GLFW vers nos KeyCode
            if (glfwKey >= 'A' && glfwKey <= 'Z') {
                return glfwKey;
            }
            if (glfwKey >= '0' && glfwKey <= '9') {
                return glfwKey;
            }

            // Mapping des touches spéciales
            static const std::unordered_map<int, int> keyMap = {
                {32, static_cast<int>(KeyCode::SPACE)},
                {256, static_cast<int>(KeyCode::ESCAPE)},
                {257, static_cast<int>(KeyCode::ENTER)},
                {258, static_cast<int>(KeyCode::TAB)},
                {259, static_cast<int>(KeyCode::BACKSPACE)},
                {260, static_cast<int>(KeyCode::INSERT)},
                {261, static_cast<int>(KeyCode::DELETE)},
                {262, static_cast<int>(KeyCode::RIGHT)},
                {263, static_cast<int>(KeyCode::LEFT)},
                {264, static_cast<int>(KeyCode::DOWN)},
                {265, static_cast<int>(KeyCode::UP)},
                {340, static_cast<int>(KeyCode::LEFT_SHIFT)},
                {341, static_cast<int>(KeyCode::LEFT_CONTROL)},
                {342, static_cast<int>(KeyCode::LEFT_ALT)},
                {45, static_cast<int>(KeyCode::MINUS)},
                {61, static_cast<int>(KeyCode::EQUAL)},
                {334, static_cast<int>(KeyCode::PLUS)},
            };

            auto it = keyMap.find(glfwKey);
            if (it != keyMap.end()) {
                return it->second;
            }

            return glfwKey; // Retourner tel quel si pas de mapping
        }

        static InputAction ConvertGLFWActions(int glfwAction) {
            switch (glfwAction) {
                case 0: return InputAction::RELEASE;
                case 1: return InputAction::PRESS;
                case 2: return InputAction::REPEAT;
                default: return InputAction::RELEASE;
            }
        }
    };
// ==================== INTÉGRATION AVEC SDL3 ====================
namespace sdl3_integration {

// Mapping des touches SDL3 vers nos KeyCode
static int ConvertSDLKeyToKeyCode(SDL_Keycode sdlKey) {
    // Lettres (SDL3 utilise SDLK_a, SDLK_b, etc.)
    if (sdlKey >= SDLK_A && sdlKey <= SDLK_Z) {
        // Convertir en majuscule pour notre enum
        return 'A' + (sdlKey - SDLK_A);
    }

    // Chiffres
    if (sdlKey >= SDLK_0 && sdlKey <= SDLK_9) {
        return '0' + (sdlKey - SDLK_0);
    }

    // Mapping des touches spéciales SDL3
    static const std::unordered_map<SDL_Keycode, int> sdlKeyMap = {
        // Touches de direction
        {SDLK_UP, static_cast<int>(KeyCode::UP)},
        {SDLK_DOWN, static_cast<int>(KeyCode::DOWN)},
        {SDLK_LEFT, static_cast<int>(KeyCode::LEFT)},
        {SDLK_RIGHT, static_cast<int>(KeyCode::RIGHT)},

        // Touches de contrôle
        {SDLK_ESCAPE, static_cast<int>(KeyCode::ESCAPE)},
        {SDLK_RETURN, static_cast<int>(KeyCode::ENTER)},
        {SDLK_TAB, static_cast<int>(KeyCode::TAB)},
        {SDLK_BACKSPACE, static_cast<int>(KeyCode::BACKSPACE)},
        {SDLK_SPACE, static_cast<int>(KeyCode::SPACE)},
        {SDLK_LSHIFT, static_cast<int>(KeyCode::LEFT_SHIFT)},
        {SDLK_RSHIFT, static_cast<int>(KeyCode::RIGHT_SHIFT)},
        {SDLK_LCTRL, static_cast<int>(KeyCode::LEFT_CONTROL)},
        {SDLK_RCTRL, static_cast<int>(KeyCode::RIGHT_CONTROL)},
        {SDLK_LALT, static_cast<int>(KeyCode::LEFT_ALT)},
        {SDLK_RALT, static_cast<int>(KeyCode::RIGHT_ALT)},

        // Touches françaises AZERTY
        {SDLK_Q, static_cast<int>(KeyCode::Q)},
        {SDLK_W, static_cast<int>(KeyCode::W)},
        {SDLK_E, static_cast<int>(KeyCode::E)},
        {SDLK_R, static_cast<int>(KeyCode::R)},
        {SDLK_T, static_cast<int>(KeyCode::T)},
        {SDLK_Y, static_cast<int>(KeyCode::Y)},
        {SDLK_U, static_cast<int>(KeyCode::U)},
        {SDLK_I, static_cast<int>(KeyCode::I)},
        {SDLK_O, static_cast<int>(KeyCode::O)},
        {SDLK_P, static_cast<int>(KeyCode::P)},
        {SDLK_A, static_cast<int>(KeyCode::A)},
        {SDLK_S, static_cast<int>(KeyCode::S)},
        {SDLK_D, static_cast<int>(KeyCode::D)},
        {SDLK_F, static_cast<int>(KeyCode::F)},
        {SDLK_G, static_cast<int>(KeyCode::G)},
        {SDLK_H, static_cast<int>(KeyCode::H)},
        {SDLK_J, static_cast<int>(KeyCode::J)},
        {SDLK_K, static_cast<int>(KeyCode::K)},
        {SDLK_L, static_cast<int>(KeyCode::L)},
        {SDLK_Z, static_cast<int>(KeyCode::Z)},
        {SDLK_X, static_cast<int>(KeyCode::X)},
        {SDLK_C, static_cast<int>(KeyCode::C)},
        {SDLK_V, static_cast<int>(KeyCode::V)},
        {SDLK_B, static_cast<int>(KeyCode::B)},
        {SDLK_N, static_cast<int>(KeyCode::N)},
        {SDLK_M, static_cast<int>(KeyCode::M)},

        // Touches de fonction
        {SDLK_F1, static_cast<int>(KeyCode::F1)},
        {SDLK_F2, static_cast<int>(KeyCode::F2)},
        {SDLK_F3, static_cast<int>(KeyCode::F3)},
        {SDLK_F4, static_cast<int>(KeyCode::F4)},
        {SDLK_F5, static_cast<int>(KeyCode::F5)},
        {SDLK_F6, static_cast<int>(KeyCode::F6)},
        {SDLK_F7, static_cast<int>(KeyCode::F7)},
        {SDLK_F8, static_cast<int>(KeyCode::F8)},
        {SDLK_F9, static_cast<int>(KeyCode::F9)},
        {SDLK_F10, static_cast<int>(KeyCode::F10)},
        {SDLK_F11, static_cast<int>(KeyCode::F11)},
        {SDLK_F12, static_cast<int>(KeyCode::F12)},

        // Touches spéciales
        {SDLK_EQUALS, static_cast<int>(KeyCode::EQUAL)},
        {SDLK_MINUS, static_cast<int>(KeyCode::MINUS)},
        {SDLK_PLUS, static_cast<int>(KeyCode::PLUS)},
        {SDLK_KP_PLUS, static_cast<int>(KeyCode::KP_ADD)},
        {SDLK_KP_MINUS, static_cast<int>(KeyCode::KP_SUBTRACT)},
    };

    auto it = sdlKeyMap.find(sdlKey);
    if (it != sdlKeyMap.end()) {
        return it->second;
    }

    return static_cast<int>(sdlKey); // Retourner tel quel si pas de mapping
}

// Conversion des actions SDL3 (SDL3 utilise des constantes différentes)
static InputAction ConvertSDLActions(Uint32 sdlEventType) {
    switch (sdlEventType) {
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            return InputAction::RELEASE;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            return InputAction::PRESS;
        // SDL3 n'a plus de KEY_REPEATED séparé
        default:
            return InputAction::RELEASE;
    }
}

// Conversion des boutons de souris SDL3
static MouseButton ConvertSDLMouseButton(Uint32 sdlButton) {
    switch (sdlButton) {
        case SDL_BUTTON_LEFT: return MouseButton::LEFT;
        case SDL_BUTTON_RIGHT: return MouseButton::RIGHT;
        case SDL_BUTTON_MIDDLE: return MouseButton::MIDDLE;
        case SDL_BUTTON_X1: return MouseButton::BUTTON_4;
        case SDL_BUTTON_X2: return MouseButton::BUTTON_5;
        default: return static_cast<MouseButton>(sdlButton);
    }
}

// Gestionnaire d'événements SDL3
class SDLEventHandler {
public:
    static void ProcessSDLEvents() {
        Input::GetInstance().BeginFrame();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ProcessEvent(event);
        }

        Input::GetInstance().EndFrame();
    }

private:
    static void ProcessEvent(const SDL_Event& event) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                std::cout << "Quit event received" << std::endl;
                break;

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                ProcessKeyEvent(event);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                ProcessMouseButtonEvent(event);
                break;

            case SDL_EVENT_MOUSE_MOTION:
                ProcessMouseMotionEvent(event);
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                ProcessMouseWheelEvent(event);
                break;

            // SDL3 a des événements de fenêtre spécifiques
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                std::cout << "Window focus gained" << std::endl;
                break;

            case SDL_EVENT_WINDOW_FOCUS_LOST:
                std::cout << "Window focus lost" << std::endl;
                break;
        }
    }

    static void ProcessKeyEvent(const SDL_Event& event) {
        KeyCode keyCode = static_cast<KeyCode>(
            ConvertSDLKeyToKeyCode(event.key.key)
        );
        InputAction action = ConvertSDLActions(event.type);

        Input::GetInstance().ProcessKeyEvent(
            keyCode,
            action,
            event.key.scancode,
            event.key.mod  // mod au lieu de mods
        );
    }

    static void ProcessMouseButtonEvent(const SDL_Event& event) {
        MouseButton button = ConvertSDLMouseButton(event.button.button);
        InputAction action = ConvertSDLActions(event.type);

        Input::GetInstance().ProcessMouseButtonEvent(
            button,
            action,
            event.button.clicks // mod au lieu de mods
        );
    }

    static void ProcessMouseMotionEvent(const SDL_Event& event) {
        // SDL3: motion.x et motion.y sont la position absolue
        // motion.xrel et motion.yrel sont le mouvement relatif
        Input::GetInstance().ProcessMouseMoveEvent(
            static_cast<float>(event.motion.x),
            static_cast<float>(event.motion.y),
            static_cast<float>(event.motion.xrel),  // delta X
            static_cast<float>(event.motion.yrel)   // delta Y
        );
    }

    static void ProcessMouseWheelEvent(const SDL_Event& event) {
        Input::GetInstance().ProcessMouseScrollEvent(
            static_cast<float>(event.wheel.x),
            static_cast<float>(event.wheel.y)
        );
    }
};

} // namespace sdl3_integration
} // namespace common

#endif // INPUT_H
