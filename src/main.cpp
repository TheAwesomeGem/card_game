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
    explicit TransformComponent(Rectangle rect_) : rect{rect_} {

    }

    Rectangle rect;
    // TODO: Use GLM Vector
};

struct RenderComponent {
    explicit RenderComponent(Texture2D texture_) : texture{texture_} {

    }

    Texture2D texture;
};

struct SlotComponent {
    explicit SlotComponent(int max_stack_count_) : max_stack_count{max_stack_count_}, stacked_entities{} {

    }

    int max_stack_count;
    std::deque<EntityId> stacked_entities;
    // TODO: Perhaps add stack offset either here or the Render Component?
};

struct DraggableComponent {
    bool is_selected = false;
    std::optional<EntityId> slot_entity = std::nullopt;
};

struct Entity {
    EntityId id;
    std::optional<TransformComponent> transform;
    std::optional<RenderComponent> render;
    std::optional<SlotComponent> slot;
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

        float dist_to_drop = RecDistSqr(rec, other_entity.transform->rect);

        if (dist_to_drop < closest_dist) {
            result = &other_entity;
            closest_dist = dist_to_drop;
        }
    }

    return result;
}

void set_to_slot(Entity& entity, const Entity& slot_entity) {
    entity.transform->rect.x = slot_entity.transform->rect.x;
    entity.transform->rect.y = slot_entity.transform->rect.y;
}

void move_to_slot(Entity& entity, Entity& new_slot) {
    Entity* old_slot = nullptr;

    if (entity.draggable->slot_entity.has_value()) {
        printf("Found old slot.\n");
        old_slot = &entities.at(*entity.draggable->slot_entity);
    }

    if (old_slot && old_slot->id != new_slot.id) {
        printf("Case where old slot exist and it is not the same as new slot.\n");
        entity.draggable->slot_entity.reset();
        old_slot->slot->stacked_entities.pop_front();
    }

    if (!(old_slot && old_slot->id == new_slot.id)) {
        printf("Case where old slot either does not exist nor it's the same slot.\n");
        entity.draggable->slot_entity = new_slot.id;
        new_slot.slot->stacked_entities.push_front(entity.id);
    }

    set_to_slot(entity, new_slot);
}

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Card Game");

    EntityId entity_id = uuid_rng();
    entities.emplace(entity_id,
                     Entity{
                             entity_id,
                             std::make_optional<TransformComponent>(Rectangle{0.0F, 0.0F, CARD_WIDTH, CARD_HEIGHT}),
                             std::make_optional<RenderComponent>(LoadTexture("card/2_of_clubs.png")),
                             std::nullopt,
                             std::make_optional<DraggableComponent>()
                     });
    entity_id = uuid_rng();
    entities.emplace(entity_id,
                     Entity{
                             entity_id,
                             std::make_optional<TransformComponent>(Rectangle{CARD_WIDTH + 5.0F, 0.0F, CARD_WIDTH, CARD_HEIGHT}),
                             std::make_optional<RenderComponent>(LoadTexture("card/3_of_clubs.png")),
                             std::nullopt,
                             std::make_optional<DraggableComponent>()
                     });
    entity_id = uuid_rng();
    entities.emplace(entity_id,
                     Entity{
                             entity_id,
                             std::make_optional<TransformComponent>(Rectangle{400.0F, 400.0F, CARD_WIDTH, CARD_HEIGHT}),
                             std::nullopt,
                             std::make_optional<SlotComponent>(1)
                     });
    entity_id = uuid_rng();
    entities.emplace(entity_id,
                     Entity{
                             entity_id,
                             std::make_optional<TransformComponent>(Rectangle{400.0F + CARD_WIDTH + 5.0F, 400.0F, CARD_WIDTH, CARD_HEIGHT}),
                             std::nullopt,
                             std::make_optional<SlotComponent>(1)
                     });
    entity_id = uuid_rng();
    entities.emplace(entity_id,
                     Entity{
                             entity_id,
                             std::make_optional<TransformComponent>(Rectangle{400.0F + (CARD_WIDTH * 2.0F) + 5.0F, 400.0F, CARD_WIDTH, CARD_HEIGHT}),
                             std::nullopt,
                             std::make_optional<SlotComponent>(1)
                     });

    for (auto& [_, entity]: entities) {
        // Entity Initialization Loop:

        if (entity.draggable && entity.transform) {
            // TODO: Set the draggable entity to the correct slots
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
                if (entity.draggable && entity.transform) {
                    // Drag System
                    if (CheckCollisionPointRec(Vector2{(float) GetMouseX(), (float) GetMouseY()}, entity.transform->rect)) {
                        if (entity.draggable->slot_entity.has_value()) {
                            const Entity& slot_entity = entities.at(*entity.draggable->slot_entity);
                            Entity& entity_to_select = entities.at(slot_entity.slot->stacked_entities.front());
                            entity_to_select.draggable->is_selected = true;
                        } else {
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
                if (entity.draggable && entity.transform) {
                    // Drop System
                    if (entity.draggable->is_selected) {
                        entity.draggable->is_selected = false;
                        Rectangle entity_rec = entity.transform->rect;

                        auto closest_slot = [entity_rec](Entity& other_entity) -> bool {
                            if (!(other_entity.slot && other_entity.transform)) {
                                return false;
                            }

                            return CheckCollisionRecs(entity_rec, other_entity.transform->rect);
                        };

                        Entity* slot_entity_to_drop = closest_entity(entity_rec, closest_slot);

                        if (slot_entity_to_drop) {
                            move_to_slot(entity, *slot_entity_to_drop);
                        } else {
                            // TODO: If they don't have an original slot, move them outside of the world view.
                            if (entity.draggable->slot_entity.has_value()) {
                                const Entity& old_slot = entities.at(*entity.draggable->slot_entity);
                                set_to_slot(entity, old_slot);
                            }
                        }

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
                    entity.transform->rect.x = (float) GetMouseX() - entity.transform->rect.width * 0.5F;
                    entity.transform->rect.y = (float) GetMouseY() - entity.transform->rect.height * 0.5F;
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

                if (entity.slot && entity.transform) {
                    // Do Draw Debug Slot
                    DrawRectangleLinesEx(entity.transform->rect, 2.0F, RED);
                }

                if (entity.render && entity.transform) {
                    // Do Render Texture
                    DrawTexturePro(
                            entity.render->texture,
                            Rectangle{0, 0, (float) entity.render->texture.width, (float) entity.render->texture.height},
                            entity.transform->rect,
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
