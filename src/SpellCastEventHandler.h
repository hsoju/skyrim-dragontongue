#pragma once

#include <shared_mutex>

class SpellCastEventHandler : public RE::BSTEventSink<RE::TESSpellCastEvent>
{
public:
	std::shared_mutex settings_mtx;

	std::map<std::string, std::vector<RE::ActorValue>> shout_values;
	std::vector<RE::ActorValue> default_actor_values = { RE::ActorValue::kHealth, RE::ActorValue::kMagicka, RE::ActorValue::kStamina };

	//float GetCurrentHealthPercentage(RE::ActorValueOwner* av_owner);

	RE::ActorValue GetDefaultActorValue(std::string& shout_name);
	RE::ActorValue GetShoutActorValue(RE::TESShout* shout);

	bool RemoveActorValue(RE::Actor* player, RE::ActorValueOwner* av_owner, RE::ActorValue value, float amount);
	
	/*
	void RemoveMultipleActorValues(RE::Actor* player, RE::ActorValueOwner* av_owner, std::vector<RE::ActorValue>& values,
		float amount);
	void ProcessEquippedHand(RE::Actor* player, RE::ActorValueOwner* av_owner, bool use_left_hand, float amount);
	*/

	bool ApplyShoutCosts(RE::TESShout* shout, float recovery_time);

	RE::HighProcessData* GetPlayerData();
	void AsyncSetupRecover(RE::TESShout* shout);
	void AsyncRecover(RE::TESShout* shout);
	
	void HandleShout(RE::TESObjectREFR* caster, RE::SpellItem* casted_power);

	void ImportFile(std::string& file_path, std::map<std::string, RE::ActorValue>& actor_value_mapping);
	void ImportSettings();

	virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* a_event, RE::BSTEventSource<RE::TESSpellCastEvent>*)
	{
		auto caster = a_event->object.get();
		if (!caster || !caster->IsPlayerRef()) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (!a_event || !a_event->spell) {
			return RE::BSEventNotifyControl::kContinue;
		}

		RE::SpellItem* casted_spell = RE::TESForm::LookupByID<RE::SpellItem>(a_event->spell);
		if (casted_spell) {
			auto spell_type = casted_spell->GetSpellType();
			if (spell_type == RE::MagicSystem::SpellType::kVoicePower) {
				HandleShout(caster, casted_spell);
			}
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	static bool Register()
	{
		static SpellCastEventHandler singleton;
		auto ScriptEventSource = RE::ScriptEventSourceHolder::GetSingleton();

		if (!ScriptEventSource) {
			logger::error("Script event source not found");
			return false;
		}

		ScriptEventSource->AddEventSink(&singleton);

		logger::info("Registered {}", typeid(singleton).name());

		singleton.ImportSettings();

		return true;
	}
};
