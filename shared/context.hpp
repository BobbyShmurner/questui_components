#pragma once

#include "key.hpp"
#include "UnsafeAny.hpp"

#include <concepts>
#include <tuple>
#include <any>

#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"

namespace QUC {

    struct RenderContext;

    template<typename RenderContextT = RenderContext>
    struct RenderContextChildDataT {
        UnsafeAny childData;
        std::optional<RenderContextT> childContext;

        template<typename T>
        T& getData() {
            if (!childData.has_value()) {
                return childData.make_any<T>();
            }
            return childData.get_any<T>();
        }

        template<typename F = std::function<UnityEngine::Transform*()>>
        constexpr RenderContextT& getChildContext(F transform) {
            if (!childContext) {
                return childContext.template emplace(transform());
            }

            return *childContext;
        }
    };

    using RenderContextChildData = RenderContextChildDataT<RenderContext>;

    struct RenderContext {
        using ChildContextKey = Key; // 64 bit number
        using ChildData = Il2CppObject*;

        /// @brief The parent transform to render on to.
        UnityEngine::Transform& parentTransform;

        // For now, if we ever need to trigger a rebuild we simply say that we need to on the component side.
        // Alternatively, any time we show a component, we call render, which will generate what we need and set everything accordingly.
        // For now, we will do this, though there are quite a few optimizations we may be able to make in the future.

        RenderContext(UnityEngine::Transform* ptr) : parentTransform(*ptr) {}
        RenderContext(UnityEngine::Transform& ref) : parentTransform(ref) {}

        RenderContext(RenderContext&&) = default;

        RenderContext& operator=(RenderContext&& other) {
            new (this) RenderContext(std::move(other));
            return *this;
        }

        auto& getChildData(ChildContextKey index) {
            return dataContext[index];
        }

#pragma region child Context Clutter
        void destroyChildContext(ChildContextKey id) {
            auto contextIt = dataContext.find(id);
            if (contextIt == dataContext.end()) return;

            auto& context = contextIt->second;

            if (!context.childContext)
                return;

            auto const& data = *context.childContext;
            if (data.parentTransform.m_CachedPtr) {
                // Destroy old context tree
                UnityEngine::Object::Destroy(data.parentTransform.get_gameObject());
            }


            dataContext.erase(contextIt);
        }
#pragma endregion

        template<bool includeParent = false>
        void destroyTree() {
            // yeah yeah I know stupid
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-bool-conversion"
            if (!&parentTransform)
                return;
#pragma clang diagnostic pop

            if (parentTransform.m_CachedPtr) {
                if constexpr (includeParent) {
                    UnityEngine::Object::Destroy(parentTransform.get_gameObject());
                } else {
                    int childCount = parentTransform.GetChildCount();
                    std::vector<UnityEngine::Transform*> transforms;
                    for (int i = 0; i < childCount; i++) {
                        auto transform = parentTransform.GetChild(i);
                        transforms.emplace_back(transform);
                    }
                    for (auto transform :transforms) {
                        UnityEngine::Object::Destroy(transform->get_gameObject());
                    }
                }
            }
            dataContext.clear();
        }

        template<bool destroyGO = true>
        void destroyChild(ChildContextKey key) {
            auto it = dataContext.find(key);
            if (it == dataContext.end()) return;

            auto& o = it->second.childContext;

            if (o && o->parentTransform.m_CachedPtr)
                UnityEngine::Object::Destroy(&o->parentTransform);

            dataContext.erase(it);
        }


    private:
        // TODO: Figure out cleaning unusued keys
        std::unordered_map<ChildContextKey, RenderContextChildData> dataContext;
    };

    // Allows both copies and references
    template<class T>
    /// @brief A concept for renderable components.
    /// @tparam T The type to check.
    concept renderable = requires (T t, RenderContext& c, RenderContextChildData& data) {
        t.render(c, data);
    } && requires(T const t) {
        {t.key} -> keyed;
    };

    template<class T, class U>
    concept pointer_type_match = std::is_pointer_v<T> && std::is_convertible_v<T, U>;

    template<class T, class U>
    concept renderable_return = requires (T t, RenderContext c, RenderContextChildData& data) {
        {t.render(c, data)} -> pointer_type_match<U>;
    };

    template<class T>
    /// @brief A concept for updatable components.
    /// @tparam T The type to check.
    concept updatable = requires (T t, RenderContext c) {
        t;
//        {t.update()};
    };

    template<class T>
    /// @brief A concept for cloneable components.
    /// @tparam T The type to check.
    concept cloneable = requires (T t, RenderContext c) {
        t;
//        {t.clone()} -> std::same_as<T>;
    };

    namespace detail {
        template<class T>
        requires (renderable<T>)
        static constexpr auto renderSingle(T& child, RenderContext& ctx) {
            auto& childData = ctx.getChildData(child.key);
            return child.render(ctx, childData);
        }

        template<size_t idx = 0, class... TArgs>
        requires ((renderable<TArgs> && ...))
        static constexpr void renderTuple(std::tuple<TArgs...>& args, RenderContext& ctx) {
            if constexpr (idx < sizeof...(TArgs)) {
                auto& child = std::get<idx>(args);
                renderSingle(child, ctx); // render child
                renderTuple<idx + 1>(args, ctx);
            }
        }

        template<typename T>
        requires (renderable<T>)
        static constexpr void renderDynamicList(std::span<T> const args, RenderContext& ctx) {
            for (auto& child : args) {
                renderSingle<T>(child, ctx); // render child
            }
        }

        template<size_t idx = 0, class... TArgs>
        requires ((cloneable<TArgs> && ...))
        std::tuple<TArgs...> cloneTuple(std::tuple<TArgs...> const& args) {
            if constexpr (idx < sizeof...(TArgs)) {
                auto clone = std::get<idx>(args).clone();
                auto newTuple = cloneTuple<idx + 1>(args);
                return std::make_tuple<TArgs...>(clone, newTuple);
            }
        }
    }
}