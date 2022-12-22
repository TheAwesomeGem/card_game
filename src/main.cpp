#include <raylib.h>


#define RAYGUI_IMPLEMENTATION

#include <raygui.h>
#include <sstream>
#include <uuid.h>
#include <ranges>
#include <deque>
#include "raymath.h"


using EntityId = uuids::uuid;

constexpr const int SCREEN_WIDTH = 1280;
constexpr const int SCREEN_HEIGHT = 720;
constexpr const float CARD_WIDTH = 111.0F;
constexpr const float CARD_HEIGHT = 160.0F;

// TODO: Figure out how to render stacked cards. Slots!

struct TransformComponent {
    explicit TransformComponent(Rectangle rec_) : rec{rec_}, last_rec{rec_} {

    }

    Rectangle rec;
    Rectangle last_rec;
    // TODO: Use GLM Vector
};

struct RenderComponent {
    explicit RenderComponent(Texture2D texture_) : texture{texture_} {

    }

    Texture2D texture;
};

struct StackData {
    explicit StackData(int max_stack_count_) : max_stack_count{max_stack_count_}, stacked_entities{} {

    }

    int max_stack_count;
    std::deque<EntityId> stacked_entities;
};

struct StackableComponent {
    StackData* stack = nullptr;
    // TODO: Perhaps add stack offset either here or the Render Component?
};

struct DraggableComponent {
    bool is_selected = false;
};

struct Entity {
    EntityId id;
    std::optional<TransformComponent> transform;
    std::optional<RenderComponent> render;
    std::optional<StackableComponent> stackable;
    std::optional<DraggableComponent> draggable;
};

static std::mt19937 rng;
static auto uuid_rng = uuids::uuid_random_generator(rng);

// TODO: These are globals which is probably bad
static std::unordered_map<EntityId, Entity> entities; // TODO: This might not be good for performance especially since we loop this quite frequently

Vector2 RecToVec(Rectangle rec) {
    return Vector2{rec.x, rec.y};
}

float RecDistSqr(Rectangle rec1, Rectangle rec2) {
    return Vector2DistanceSqr(RecToVec(rec1), RecToVec(rec2));
}

Entity* closest_entity(Rectangle rec, const std::function<bool(Entity&)>& predicate) {
    float closest_dist = std::numeric_limits<float>::max();
    Entity* result = nullptr;

    for (auto& [_, other_entity]: entities) {
        if (!predicate(other_entity)) {
            continue;
        }

        float dist_to_drop = RecDistSqr(rec, other_entity.transform->rec);

        if (dist_to_drop < closest_dist) {
            result = &other_entity;
            closest_dist = dist_to_drop;
        }
    }

    return result;
}

static void move_to_entity(Entity& entity, const Entity& other_entity) {
    entity.transform->rec.x = other_entity.transform->rec.x;
    entity.transform->rec.y = other_entity.transform->rec.y;
}

static void stack_to_entity(Entity& entity_to_stack, Entity& other_entity) {
    if (entity_to_stack.stackable->stack != nullptr && other_entity.stackable->stack != nullptr && entity_to_stack.stackable->stack == other_entity.stackable->stack) {
        printf("Stacked to the same entity. No change required.\n");

        return;
    }

    if (entity_to_stack.stackable->stack) {
        StackData* stack = entity_to_stack.stackable->stack;
        entity_to_stack.stackable->stack = nullptr;
        stack->stacked_entities.pop_front(); // Assume that the last selected entity is always at the front of the stack.

        if (stack->stacked_entities.size() < 2) {
            entities.at(stack->stacked_entities.front()).stackable->stack = nullptr;
            stack->stacked_entities.clear();
            printf("Destructing the stack.\n");
            delete stack; // Could use shared pointer here.
        }

        printf("Removing entity from old stack.\n");
    }

    if (other_entity.stackable->stack) {
        StackData* stack = other_entity.stackable->stack;
        stack->stacked_entities.push_front(entity_to_stack.id);
        entity_to_stack.stackable->stack = stack;

        printf("The new stack exists so adding this entity to the new stack.\n");
    } else {
        StackData* stack = new StackData{40}; // TODO: Change the max stack amount based on the context.
        stack->stacked_entities.push_front(other_entity.id);
        stack->stacked_entities.push_front(entity_to_stack.id);
        entity_to_stack.stackable->stack = stack;
        other_entity.stackable->stack = stack;

        printf("Stack does not exist. Creating a new one.\n");
    }

    move_to_entity(entity_to_stack, other_entity);
}

static Entity* get_top_entity_from_stack(StackData* stack) {
    return &entities.at(stack->stacked_entities.front());
}

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Card Game");

    EntityId entity_id = uuid_rng();
    entities.emplace(entity_id,
                     Entity{
                             entity_id,
                             std::make_optional<TransformComponent>(Rectangle{0.0F, 0.0F, CARD_WIDTH, CARD_HEIGHT}),
                             std::make_optional<RenderComponent>(LoadTexture("card/2_of_clubs.png")),
                             std::make_optional<StackableComponent>(),
                             std::make_optional<DraggableComponent>()
                     });
    entity_id = uuid_rng();
    entities.emplace(entity_id,
                     Entity{
                             entity_id,
                             std::make_optional<TransformComponent>(Rectangle{CARD_WIDTH + 5.0F, 0.0F, CARD_WIDTH, CARD_HEIGHT}),
                             std::make_optional<RenderComponent>(LoadTexture("card/3_of_clubs.png")),
                             std::make_optional<StackableComponent>(),
                             std::make_optional<DraggableComponent>()
                     });
    entity_id = uuid_rng();
    entities.emplace(entity_id,
                     Entity{
                             entity_id,
                             std::make_optional<TransformComponent>(Rectangle{400.0F, 400.0F, CARD_WIDTH, CARD_HEIGHT}),
                             std::nullopt,
                             std::make_optional<StackableComponent>()
                     });
    entity_id = uuid_rng();
    entities.emplace(entity_id,
                     Entity{
                             entity_id,
                             std::make_optional<TransformComponent>(Rectangle{400.0F + CARD_WIDTH + 5.0F, 400.0F, CARD_WIDTH, CARD_HEIGHT}),
                             std::nullopt,
                             std::make_optional<StackableComponent>()
                     });
    entity_id = uuid_rng();
    entities.emplace(entity_id,
                     Entity{
                             entity_id,
                             std::make_optional<TransformComponent>(Rectangle{400.0F + (CARD_WIDTH * 2.0F) + 5.0F, 400.0F, CARD_WIDTH, CARD_HEIGHT}),
                             std::nullopt,
                             std::make_optional<StackableComponent>()
                     });

    for (auto& [_, entity]: entities) {
        // Entity Initialization Loop:

        if (entity.draggable && entity.transform) {

        }
    }

    SetTargetFPS(60);

    bool left_clicked = false;

    while (!WindowShouldClose()) {
        std::stringstream text_stream{"Card: ", std::ios_base::app | std::ios_base::out};

        // Event
        if (!left_clicked && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            left_clicked = true;
            // User just pressed left click

            for (auto& [_, entity]: entities) {
                if (entity.draggable && entity.transform && entity.stackable) {
                    // Drag System
                    if (CheckCollisionPointRec(Vector2{(float) GetMouseX(), (float) GetMouseY()}, entity.transform->rec)) {
                        if (entity.stackable.has_value() && entity.stackable->stack) {
                            Entity* entity_to_select = get_top_entity_from_stack(entity.stackable->stack);

                            if (entity_to_select) {
                                entity_to_select->transform->last_rec = entity_to_select->transform->rec;
                                entity_to_select->draggable->is_selected = true;
                            } else {
                                printf("Something terrible happened.\n");
                            }
                        } else {
                            entity.transform->last_rec = entity.transform->rec;
                            entity.draggable->is_selected = true;
                        }

                        break;
                    }
                    // =========
                }
            }
        } else if (left_clicked && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            left_clicked = false;
            // User just released left click

            for (auto& [_, entity]: entities) {
                if (entity.draggable && entity.transform && entity.stackable) {
                    // Drop System
                    if (entity.draggable->is_selected) {
                        entity.draggable->is_selected = false;

                        auto closest_stackable = [&entity](Entity& other_entity) -> bool {
                            if (entity.id == other_entity.id) {
                                return false;
                            }

                            if (!(other_entity.stackable && other_entity.transform)) {
                                return false;
                            }

                            return CheckCollisionRecs(entity.transform->rec, other_entity.transform->rec);
                        };

                        Entity* stackable_entity = closest_entity(entity.transform->rec, closest_stackable);

                        if (stackable_entity) {
                            stack_to_entity(entity, *stackable_entity);
                        } else {
                            printf("Cannot find stackable entity so moved to last known position.\n");
                            entity.transform->rec = entity.transform->last_rec;
                        }

                        entity.transform->last_rec = entity.transform->rec;
                        // TODO: Trigger an event or a callback or a state change when card was dropped.
                    }
                    // ==========
                }
            }
        }


        // Update
        for (auto& [_, entity]: entities) {
            // Entity Update Loop:
            // TODO: Needs priority for the system update ordering

            if (entity.draggable && entity.transform) {
                if (entity.draggable->is_selected) {
                    text_stream << "True ";
                    entity.transform->rec.x = (float) GetMouseX() - entity.transform->rec.width * 0.5F;
                    entity.transform->rec.y = (float) GetMouseY() - entity.transform->rec.height * 0.5F;
                } else {
                    text_stream << "False ";
                }
            }
        }

        // Draw
        BeginDrawing();
        {
            ClearBackground(LIGHTGRAY);

            for (auto& [_, entity]: entities) {
                // Entity Draw Loop:
                // TODO: Needs priority for the system update drawing

                // TODO: Render priority based on stackable

                if (entity.stackable && entity.transform) {
                    // Do Draw Debug Slot
                    DrawRectangleLinesEx(entity.transform->rec, 2.0F, RED);
                }

                if (entity.render && entity.transform) {
                    // Do Render Texture
                    DrawTexturePro(
                            entity.render->texture,
                            Rectangle{0, 0, (float) entity.render->texture.width, (float) entity.render->texture.height},
                            entity.transform->rec,
                            Vector2{0.0F, 0.0F},
                            0.0F,
                            WHITE
                    );
                }
            }

            DrawText(text_stream.str().c_str(), 800 - 200, 100, 18, DARKBLUE);
        }
        EndDrawing();
    }

    for (auto& [_, entity]: entities) {
        // Entity Destruct Loop:
        // TODO: Needs priority for the system destructing

        if (entity.render) {
            // Destruct Render Texture
            UnloadTexture(entity.render->texture);
        }
    }

    CloseWindow();

    return 0;
}
