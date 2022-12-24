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
constexpr const float CARD_WIDTH = 83.25F;
constexpr const float CARD_HEIGHT = 120.0F;
constexpr const int MAX_Z_INDEX = 1000;

// TODO: Now there is another bug where we cannot update 'at_top_z' since we move away from that card.
// TODO: Find a different solution to handle stacking. The 'z' system is not working. We need a more complex solution sadly.
// TODO: Add more generators
// TODO: Just position still be window space or should it be a normalized world space? Figure out sizing, coordinate space, positioning

struct TransformComponent {
    explicit TransformComponent(Rectangle rec_) : rec{rec_}, last_rec{rec_}, z{0}, last_z{0}, at_top_z{true} {

    }

    Rectangle rec;
    Rectangle last_rec;
    int last_z;
    int z;
    bool at_top_z; // TODO: This is a temp solution to make sure you cannot place a card unless it's at the top of the stack. In the future allow cards to be placed in between

    // TODO: Use GLM Vector
};

struct RenderComponent {
    explicit RenderComponent(std::optional<Texture2D> texture_, float offset_x_, float offset_y_) : texture{texture_}, offset_x{offset_x_}, offset_y{offset_y_} {

    }

    std::optional<Texture2D> texture;
    float offset_x;
    float offset_y;
};

struct DraggableComponent {
    bool is_selected = false;
};

struct Entity {
    EntityId id;
    std::optional<TransformComponent> transform;
    std::optional<RenderComponent> render;
    std::optional<DraggableComponent> draggable;
};

static std::mt19937 rng;
static auto uuid_rng = uuids::uuid_random_generator(rng);

// TODO: These are globals which is probably bad
static std::unordered_map<EntityId, Entity> entities; // TODO: This might not be good for performance especially since we loop this quite frequently

Vector2 RecToVec(Rectangle rec) {
    return Vector2{rec.x - rec.width * 0.5F, rec.y - rec.height * 0.5F};
}

float RecDistSqr(Rectangle rec1, Rectangle rec2) {
    return Vector2DistanceSqr(RecToVec(rec1), RecToVec(rec2));
}

bool close_to(float a, float b, float epsilon) {
    return fabs(a - b) <= ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

static Rectangle get_screen_rec(Entity* entity) {
    Rectangle dest{entity->transform->rec};

    if (entity->transform->z < MAX_Z_INDEX) {
        dest.x += entity->render->offset_x * (float) (std::max(entity->transform->z - 1, 0));
        dest.y += entity->render->offset_y * (float) (std::max(entity->transform->z - 1, 0));
    }

    return dest;
}

Entity* top_entity(const std::function<bool(Entity&)>& predicate) {
    int z_index = 0.0F;
    Entity* result = nullptr;

    for (auto& [_, other_entity]: entities) {
        if (!predicate(other_entity)) {
            continue;
        }

        if (other_entity.transform->z >= z_index) {
            result = &other_entity;
            z_index = other_entity.transform->z;
        }
    }

    return result;
}

// TODO: Is there a way to combine these two loops?
Entity* closest_entity(Rectangle rec, const std::function<bool(Entity&)>& predicate) {
    float closest_dist = std::numeric_limits<float>::max();
    Vector2 close_vec;

    for (auto& [_, other_entity]: entities) {
        if (!predicate(other_entity)) {
            continue;
        }

        Rectangle other_rec = get_screen_rec(&other_entity);
        float dist_to_drop = RecDistSqr(rec, other_rec);

        if (dist_to_drop < closest_dist) {
            close_vec = RecToVec(other_rec);
            closest_dist = dist_to_drop;
        }
    }

    Entity* result = nullptr;
    int z_index = 0;
    for (auto& [_, other_entity]: entities) {
        if (!predicate(other_entity)) {
            continue;
        }

        Vector2 other_vec = RecToVec(get_screen_rec(&other_entity));

        // TODO: Create a function that checks if two vecs are close to each other.
        if (!(close_to(close_vec.x, other_vec.x, 0.1F) && close_to(close_vec.y, other_vec.y, 0.1F))) {
            continue;
        }

        if (other_entity.transform->z >= z_index) {
            result = &other_entity;
            z_index = other_entity.transform->z;
        }
    }

    return result;
}

static void move_to_entity(Entity& entity, const Entity& other_entity) {
    entity.transform->rec.x = other_entity.transform->rec.x;
    entity.transform->rec.y = other_entity.transform->rec.y;
}

static void stack_to_entity(Entity& entity_to_stack, Entity& other_entity) {
    entity_to_stack.transform->z = other_entity.transform->z + 1;
    other_entity.transform->at_top_z = false;
    entity_to_stack.transform->at_top_z = true;

    if (entity_to_stack.render.has_value()) {
        entity_to_stack.render->offset_x = other_entity.render->offset_x;
        entity_to_stack.render->offset_y = other_entity.render->offset_y;
    }

    move_to_entity(entity_to_stack, other_entity);
}

static Entity& create_card(Vector2 position, std::string_view texture_name) {
    EntityId entity_id = uuid_rng();
    entities.emplace(entity_id,
                     Entity{
                             entity_id,
                             std::make_optional<TransformComponent>(Rectangle{position.x, position.y, CARD_WIDTH, CARD_HEIGHT}),
                             std::make_optional<RenderComponent>(std::make_optional<>(LoadTexture(std::format("card/{}.png", texture_name).c_str())), 0.0F, 0.0F),
                             std::make_optional<DraggableComponent>()
                     });

    return entities.at(entity_id);
}

static Entity& create_slot(Vector2 position, float offset_x, float offset_y) {
    EntityId entity_id = uuid_rng();
    entities.emplace(entity_id,
                     Entity{
                             entity_id,
                             std::make_optional<TransformComponent>(Rectangle{position.x, position.y, CARD_WIDTH, CARD_HEIGHT}),
                             std::make_optional<RenderComponent>(std::nullopt, offset_x, offset_y),
                     });

    return entities.at(entity_id);
}

static void create_row_of_cards_with_slots(Vector2 start_position, uint8_t num_of_cards) {
    for (uint8_t i = 0; i < num_of_cards; ++i) {
        Entity& entity_slot = create_slot(start_position, 0.0F, -16.0F);
        Entity& entity_card = create_card(Vector2{}, std::format("{}_of_clubs", std::to_string(i + 2)));
        stack_to_entity(entity_card, entity_slot);

        start_position.x += CARD_WIDTH + 4.0F;
    }
}

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Card Game");

    create_row_of_cards_with_slots(Vector2{300.0F, 400.0F}, 9);

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

            // Drag System
            auto closest_draggable = [](Entity& other_entity) -> bool {
                // TODO: Handle entities that doesn't stack in the future.
                if (!(other_entity.transform && other_entity.draggable)) {
                    return false;
                }

                if (!other_entity.transform->at_top_z) {
                    return false;
                }

                // TODO: Create a function that creates a rectangle from mouse cursor with some arbitrary size
                return CheckCollisionPointRec(Vector2{(float) GetMouseX(), (float) GetMouseY()}, get_screen_rec(&other_entity));
            };

            Entity* entity_to_select = top_entity(closest_draggable);

            if (entity_to_select) {
                entity_to_select->transform->last_rec = entity_to_select->transform->rec;
                entity_to_select->transform->last_z = entity_to_select->transform->z;
                entity_to_select->transform->z = MAX_Z_INDEX;
                entity_to_select->draggable->is_selected = true;
            }
            // =========
        } else if (left_clicked && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            left_clicked = false;
            // User just released left click

            for (auto& [_, entity]: entities) {
                if (entity.draggable && entity.transform) {
                    // Drop System
                    if (entity.draggable->is_selected) {
                        entity.draggable->is_selected = false;

                        auto closest_stackable = [&entity](Entity& other_entity) -> bool {
                            if (entity.id == other_entity.id || !other_entity.transform) {
                                return false;
                            }

                            if (!other_entity.transform->at_top_z) {
                                return false;
                            }

                            return CheckCollisionRecs(get_screen_rec(&entity), get_screen_rec(&other_entity));
                        };

                        Entity* stackable_entity = closest_entity(get_screen_rec(&entity), closest_stackable);

                        // TODO: Handle entities that doesn't stack in the future.
                        if (stackable_entity) {
                            stack_to_entity(entity, *stackable_entity);
                        } else {
                            printf("Cannot find stackable entity so moved to last known position.\n");
                            entity.transform->rec = entity.transform->last_rec;
                            entity.transform->z = entity.transform->last_z;
                        }

                        entity.transform->last_rec = entity.transform->rec;
                        entity.transform->last_z = entity.transform->z;
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

        std::vector<Entity*> drawable_entities;

        for (auto& [_, entity]: entities) {
            if (entity.transform.has_value() && entity.render.has_value()) {
                drawable_entities.push_back(&entity);
            }
        }

        std::sort(drawable_entities.begin(), drawable_entities.end(), [](Entity* entity1, Entity* entity2) {
            return entity1->transform->z < entity2->transform->z;
        });

        // Draw
        BeginDrawing();
        {
            ClearBackground(LIGHTGRAY);

            // Entity Draw Loop:
            for (Entity* entity: drawable_entities) {
                if (entity->render->texture.has_value()) {
                    // Do Render Texture
                    DrawTexturePro(
                            *entity->render->texture,
                            Rectangle{0, 0, (float) entity->render->texture->width, (float) entity->render->texture->height},
                            get_screen_rec(entity),
                            Vector2{0.0F, 0.0F},
                            0.0F,
                            WHITE
                    );
                } else {
                    // Do Draw Debug Slot
                    DrawRectangleLinesEx(entity->transform->rec, 2.0F, RED);
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
            UnloadTexture(*entity.render->texture);
        }
    }

    CloseWindow();

    return 0;
}
