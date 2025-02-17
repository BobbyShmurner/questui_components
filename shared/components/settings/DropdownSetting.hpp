#pragma once

#include "UnityEngine/Vector2.hpp"

#include "BaseSetting.hpp"

#include "HMUI/SimpleTextDropdown.hpp"
#include "System/Collections/Generic/IReadOnlyList_1.hpp"

#include "shared/context.hpp"
#include "shared/state.hpp"

#include "questui/shared/BeatSaberUI.hpp"
#include "config-utils/shared/config-utils.hpp"
#include <string>
#include <array>

namespace QUC {

    template<size_t sz, typename Container = std::array<std::string, sz>>
    struct DropdownSetting {
    protected:
        struct RenderDropdownData {
            HMUI::SimpleTextDropdown* dropdown;
            TMPro::TextMeshProUGUI* uiText;
        };
    public:

//        static_assert(renderable<DropdownSetting>);
        using OnCallback = std::function<void(DropdownSetting&, std::string const&, UnityEngine::Transform *, RenderContext& ctx)>;
        HeldData<std::string> text;
        OnCallback callback;
        HeldData<bool> enabled;
        HeldData<bool> interactable;
        HeldData<std::string> value;
        HeldData<Container> values;

        const Key key;

        template<class F>
        constexpr DropdownSetting(std::string_view txt, std::string_view current, F &&callable,
                                  Container v = Container(), bool enabled_ = true,
                                  bool interact = true)
                : text(txt), callback(callable), enabled(enabled_), interactable(interact), value(current),
                  values(v) {}

        UnityEngine::Transform* render(RenderContext& ctx, RenderContextChildData& data) {
            auto& settingData = data.getData<RenderDropdownData>();
            auto& dropdown = settingData.dropdown;

            auto parent = &ctx.parentTransform;
            if (!dropdown) {
                auto cbk = [callback = this->callback, parent, &ctx, this](StringW val) mutable {
                    value = static_cast<std::string>(val);
                    value.clear();

                    if (callback)
                        callback(*this, value.getData(), parent, ctx);
                };
                std::vector<StringW> nonsense(values.getData().begin(), values.getData().end());
                dropdown = QuestUI::BeatSaberUI::CreateDropdown(parent, *text, *value, nonsense, cbk);
                text.clear();
                value.clear();
                values.clear();
                assign<true>(settingData);
            } else {
                assign<false>(settingData);
            }


            return dropdown->get_transform();
        }

        [[nodiscard]] std::string const& getValue() const {
            return *value;
        }

        void setValue(std::string_view val) {
            value = val;
        }

        void update(RenderContext& ctx) {
            auto& data = ctx.getChildData(key);
            auto& renderDropdownData = data.getData<RenderDropdownData>();

            assign<false>(renderDropdownData);
        }

    protected:
        template<bool created>
        void assign(RenderDropdownData& renderDropdownData) {
            auto& dropdown = renderDropdownData.dropdown;
            auto& uiText = renderDropdownData.uiText;
            CRASH_UNLESS(dropdown);

            if (enabled) {
                dropdown->set_enabled(*enabled);
                enabled.clear();
            }

            if (!*enabled) {
                // Don't bother setting anything if we aren't enabled.
                return;
            }

            if constexpr (created) {
                dropdown->button->set_interactable(*interactable);
                interactable.clear();
            } else if (interactable) {
                dropdown->button->set_interactable(*interactable);
                interactable.clear();
            }

            if constexpr (!created) {
                if (text) {
                    if (!uiText) {
                        // From QuestUI
                        static auto labelName = il2cpp_utils::newcsstr<il2cpp_utils::CreationType::Manual>("Label");
                        auto labelTransform = dropdown->get_transform()->get_parent()->Find(labelName);
                        if (labelTransform) {
                            UnityEngine::GameObject *labelObject = labelTransform->get_gameObject();
                            if (labelObject) {
                                uiText = labelObject->GetComponent<TMPro::TextMeshProUGUI *>();
                            }
                        }
                    }

                    uiText->set_text(il2cpp_utils::newcsstr(*text));
                    text.clear();
                }

                if (value || values) {
                    List<StringW>* list = nullptr;

                    if (values)
                        list = List<StringW>::New_ctor();

                    int selectedIndex = 0;
                    for (int i = 0; i < values.getData().size(); i++) {
                        std::string const &dropdownValue = values.getData()[i];
                        if (value.getData() == dropdownValue) {
                            selectedIndex = i;
                        }
                        if (list)
                            list->Add(dropdownValue);
                    }

                    if (list) {
                        dropdown->SetTexts(
                                reinterpret_cast<System::Collections::Generic::IReadOnlyList_1<StringW> *>(list));
                    }

                    if (dropdown->selectedIndex != selectedIndex)
                        dropdown->SelectCellWithIdx(selectedIndex);

                    value.clear();
                    values.clear();
                }
            }
        }
    };

    using VariableDropdownSetting = DropdownSetting<0, std::vector<std::string>>;
    static_assert(IsConfigType<VariableDropdownSetting, std::string>);

// TODO: Test if it works
#pragma region ConfigEnum
#if defined(AddConfigValue) || __has_include("config-utils/shared/config-utils.hpp")
    template<typename EnumType>
    using EnumToStrType = std::unordered_map<EnumType, std::string>;

    template<typename EnumType>
    using StrToEnumType = std::unordered_map<std::string, EnumType>;

    template<typename EnumType>
    struct EnumToStr;

    template<typename EnumType>
    struct StrToEnum;

    template<typename EnumType>
    struct EnumStrValues;


    template<typename EnumType>
    EnumToStrType<EnumType>
    createFromKeysAndValues(std::initializer_list<int> keysList, std::initializer_list<std::string> valuesList) {
        std::vector<int> keys(keysList);
        std::vector<std::string> values(valuesList);
        EnumToStrType<EnumType> map(keys.size());
        for (int i = 0; i < keys.size(); i++) {
            map.emplace((EnumType) keys[i], values[i]);
        }

        return map;
    }


    template<typename EnumType>
    StrToEnumType<EnumType>
    createFromKeysAndValues(std::initializer_list<std::string> keysList, std::initializer_list<int> valuesList) {
        std::vector<std::string> keys(keysList);
        std::vector<int> values(valuesList);
        StrToEnumType<EnumType> map(keys.size());
        for (int i = 0; i < keys.size(); i++) {
            map.emplace(keys[i], (EnumType) values[i]);
        }

        return map;
    }

#define STR_LIST(...) __VA_ARGS__

#define DROPDOWN_CREATE_ENUM_CLASS(EnumName, strlist, ...) \
enum struct EnumName {                            \
    __VA_ARGS__                                   \
};                                                \
namespace QUC {             \
enum FakeEnum__##EnumName {                       \
    __VA_ARGS__                                   \
};                                                         \
template<> struct ::QUC::EnumToStr<EnumName> {                      \
    inline static const EnumToStrType<EnumName> map = createFromKeysAndValues<EnumName>({__VA_ARGS__}, {strlist}); \
    static EnumToStrType<EnumName> get() {                                       \
        return map;                                          \
    }                                              \
};                                                \
template<> struct ::QUC::StrToEnum<EnumName> {                      \
    inline static const QUC::StrToEnumType<EnumName> map = createFromKeysAndValues<EnumName>({strlist}, {__VA_ARGS__}); \
    static StrToEnumType<EnumName> get() {                                       \
        return map;                                          \
    }                                              \
};                                                \
template<> struct ::QUC::EnumStrValues<EnumName> {                      \
    inline static const std::vector<std::string> values = std::vector<std::string>({strlist}); \
    static std::vector<std::string> get() {                                       \
        return values;                                          \
    }                                              \
};                                                \
} // end namespace EnumNamespace__##EnumName


    // c++ inheritance is a pain
    template<typename EnumType, size_t sz, typename EnumConfigValue = int, bool CrashOnBoundsExit = false>
    requires(std::is_enum_v<EnumType>)
    struct ConfigUtilsEnumDropdownSetting : public DropdownSetting<sz, std::vector<std::string>> {
        using SettingType = DropdownSetting<sz, std::vector<std::string>>;

        static_assert(&EnumToStr<EnumType>::get, "Please create a type specialization for EnumToStr");
        static_assert(&StrToEnum<EnumType>::get, "Please create a type specialization for StrToEnum");
        static_assert(&EnumStrValues<EnumType>::get, "Please create a type specialization for EnumStrValues");

        template<typename... TArgs>
        explicit
        ConfigUtilsEnumDropdownSetting(ConfigUtils::ConfigValue<EnumConfigValue> &configValue, TArgs &&... args)
                : configValue(configValue),
                  SettingType(configValue.GetName(), "", buildCallback(configValue), EnumStrValues<EnumType>::values, args...) {

        }

        template<typename F, typename... TArgs>
        explicit
        ConfigUtilsEnumDropdownSetting(ConfigUtils::ConfigValue<EnumConfigValue> &configValue, F&& callback, TArgs &&... args)
                : configValue(configValue),
                  SettingType(configValue.GetName(), "", buildCallback<F>(std::forward<F>(callback), configValue), EnumStrValues<EnumType>::values, args...) {

        }

        const Key key;

        UnityEngine::Transform* render(RenderContext& ctx, RenderContextChildData& data) {
            SettingType::setValue(getValue());
            auto& hoverHint = data.getData<HMUI::HoverHint*>();
            Key parentKey = SettingType::key;
            auto res = SettingType::render(ctx, ctx.getChildData(parentKey));

            if (!configValue.GetHoverHint().empty() && !hoverHint) {
                hoverHint = QuestUI::BeatSaberUI::AddHoverHint(res->get_gameObject(), configValue.GetHoverHint());
            }

            return res;
        };

        std::string_view getValue() {
            return getValue(configValue);
        }

        static std::string_view getValue(ConfigUtils::ConfigValue<int>& configValue) {
            EnumToStrType<EnumType> const& map = EnumToStr<EnumType>::map;
            if constexpr (CrashOnBoundsExit) {
                return map.at((EnumType) configValue);
            } else {
                auto it = map.find((EnumType) configValue.GetValue());

                if (it == map.end()) {
                    if (map.size() == 0) return "";
                    else return map.cbegin()->second;
                }

                return it->second;
            }
        };

        void setValue(const std::string &val) {
            return setValue(val, configValue);
        }

        static void setValue(const std::string &val, ConfigUtils::ConfigValue<int>& configValue) {
            StrToEnumType<EnumType> const& map = StrToEnum<EnumType>::map;
            if constexpr (CrashOnBoundsExit) {
                EnumType newValue = map.at(val);
                configValue.SetValue((int) newValue);
            } else {
                auto it = map.find(val);

                if (it == map.end()) {
                    if (map.size() == 0) configValue.SetValue((int) 0);
                    else configValue.SetValue((int) map.cbegin()->second);
                }

                configValue.SetValue((int) it->second);
            }
        }

    protected:
        // reference capture should be safe here
        ConfigUtils::ConfigValue<int> &configValue;

        static typename SettingType::OnCallback buildCallback(ConfigUtils::ConfigValue<EnumConfigValue>& configValue, typename SettingType::OnCallback const& callback) {
            return [callback, &configValue](auto& setting, std::string const& val, UnityEngine::Transform* t, RenderContext& ctx) -> typename SettingType::OnCallback::result_type {
                configValue.SetValue(val);
                if (callback) {
                    return callback(setting, val, t, ctx);
                } else {
                    return {};
                }
            };
        }

        template<typename F = typename SettingType::OnCallback const&>
        static typename SettingType::OnCallback buildCallback(ConfigUtils::ConfigValue<EnumConfigValue>& configValue, F callback) {
            return [callback, &configValue](auto& setting, std::string const& val, UnityEngine::Transform* t, RenderContext& ctx) -> typename SettingType::OnCallback::result_type {
                configValue.SetValue(val);

                return callback(setting, val, t, ctx);
            };
        }

        static typename SettingType::OnCallback buildCallback(ConfigUtils::ConfigValue<EnumConfigValue>& configValue) {
            return [&configValue](auto& setting, std::string const& val, UnityEngine::Transform* t, RenderContext& ctx) -> typename SettingType::OnCallback::result_type {
                setValue(val, configValue);
                return {};
            };
        }
    };

#endif
#pragma endregion
}