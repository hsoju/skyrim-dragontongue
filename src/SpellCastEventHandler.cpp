#include <locale>
#include <codecvt>
#include <functional>

#include <SimpleIni.h>

#include "SpellCastEventHandler.h"

/*
float SpellCastEventHandler::GetCurrentHealthPercentage(RE::ActorValueOwner* av_owner) {
	return av_owner->GetActorValue(RE::ActorValue::kHealth) / av_owner->GetPermanentActorValue(RE::ActorValue::kHealth);
}
*/

RE::ActorValue SpellCastEventHandler::GetDefaultActorValue(std::string& shout_name)
{
	std::hash<std::string> hasher;
	auto hashed = hasher(shout_name);
	return default_actor_values[(hashed % default_actor_values.size())];
}

RE::ActorValue SpellCastEventHandler::GetShoutActorValue(RE::TESShout* shout) {
	auto player = RE::PlayerCharacter::GetSingleton()->As<RE::Actor>();
	auto av_owner = player->AsActorValueOwner();
}

bool SpellCastEventHandler::RemoveActorValue(RE::Actor* player, RE::ActorValueOwner* av_owner, RE::ActorValue value, 
	float amount) {
	float has_remaining_av = true;
	float current_value = av_owner->GetActorValue(value);
	if (value == RE::ActorValue::kHealth) {
		float possible_value = current_value - amount;
		if (possible_value < 0.0f) {
			amount = max(current_value - 1.0f, 0.0f);
			has_remaining_av = false;
		}
	}
	player->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, value, amount * -1);
	return has_remaining_av && ((amount - current_value) < 0.0f);
}

/*
void SpellCastEventHandler::RemoveMultipleActorValues(RE::Actor* player, RE::ActorValueOwner* av_owner, 
	std::vector<RE::ActorValue>& values, float amount) {
	for (RE::ActorValue value : values) {
		float remaining_amount = RemoveActorValue(player, av_owner, value, amount);
		if (remaining_amount > 0.0f) {
			amount = remaining_amount;
		} else {
			break;
		}
	}
}

void SpellCastEventHandler::ProcessEquippedHand(RE::Actor* player, RE::ActorValueOwner* av_owner, bool use_left_hand, 
	float amount)
{
	std::vector<RE::ActorValue> values_to_remove;
	auto current_hand = player->GetEquippedObject(use_left_hand);
	if (current_hand) {
		if (current_hand->GetFormType() == RE::FormType::Spell) {
			values_to_remove.assign({ RE::ActorValue::kStamina, RE::ActorValue::kMagicka, RE::ActorValue::kHealth });
		} else {
			values_to_remove.assign({ RE::ActorValue::kMagicka, RE::ActorValue::kStamina, RE::ActorValue::kHealth });
		}
	} else {
		values_to_remove.assign({ RE::ActorValue::kMagicka, RE::ActorValue::kStamina, RE::ActorValue::kHealth });
	}
	RemoveMultipleActorValues(player, av_owner, values_to_remove, amount * 0.75f);
}
*/

void SpellCastEventHandler::ApplyMatchedShoutCosts(std::string& shout_identifier, float recovery_time, RE::Actor* player, 
	RE::ActorValueOwner* av_owner, bool& has_remaining_av)
{
	auto& given_values = shout_values[shout_identifier];
	auto num_values = given_values.size();
	if (num_values > 0) {
		for (int idx = 0; idx < num_values; idx++) {
			bool remaining = RemoveActorValue(player, av_owner, given_values[idx], recovery_time / num_values);
			has_remaining_av = has_remaining_av || remaining;
		}
	} else {
		has_remaining_av = RemoveActorValue(player, av_owner, GetDefaultActorValue(shout_identifier), recovery_time);
	}
}

bool SpellCastEventHandler::ApplyShoutCosts(RE::TESShout* shout, float recovery_time) {
	bool has_remaining_av = false;
	auto player = RE::PlayerCharacter::GetSingleton()->As<RE::Actor>();
	auto av_owner = player->AsActorValueOwner();
	std::string shout_name = shout->GetName();
	std::string shout_editor_id = shout->GetFormEditorID();
	if (shout_values.contains(shout_name)) {
		ApplyMatchedShoutCosts(shout_name, recovery_time, player, av_owner, has_remaining_av);
	} else {
		has_remaining_av = RemoveActorValue(player, av_owner, GetDefaultActorValue(shout_name), recovery_time);
	}
	return has_remaining_av;
}

float SpellCastEventHandler::CalculateShoutCosts(RE::TESShout* shout, float shout_time, float recovery_time) {
	float base_cost = 10.0f;
	float time_difference = shout_time - previous_shout_time;
	//float time_cost = (shout == previous_shout) ? 

}

float SpellCastEventHandler::GetTimeOfShout() {
	auto calendar = RE::Calendar::GetSingleton();
	if (calendar) {
		return calendar->GetCurrentGameTime() * 3600 * 24 / calendar->GetTimescale();
	}
	return 0.0f;
}

RE::HighProcessData* SpellCastEventHandler::GetPlayerData()
{
	auto player = RE::PlayerCharacter::GetSingleton()->As<RE::Actor>();
	auto process = player->GetActorRuntimeData().currentProcess;
	if (process) {
		auto high_data = process->high;
		if (high_data) {
			return high_data;
		}
	}
	return nullptr;
}

void SpellCastEventHandler::AsyncSetupRecover(RE::TESShout* shout)
{
	auto ThreadFunc = [this](RE::TESShout* shout) -> void {
		AsyncRecover(shout);
	};
	std::jthread thread(ThreadFunc, shout);
	thread.detach();
}

void SpellCastEventHandler::AsyncRecover(RE::TESShout* shout)
{
	float delay = 10.0f;
	std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delay)));
	const auto task_interface = SKSE::GetTaskInterface();

	if (task_interface != nullptr) {
		auto player = RE::PlayerCharacter::GetSingleton();
		if (player && player->GetActorRuntimeData().currentProcess &&
			player->GetActorRuntimeData().currentProcess->InHighProcess()) {
			task_interface->AddTask([this, player, shout]() -> void {
				auto player_data = GetPlayerData();
				float current_time = GetTimeOfShout();
				float current_recovery_time = player_data->voiceRecoveryTime;
				bool has_remaining_av = ApplyShoutCosts(shout, current_recovery_time + 10.0f);
				if (has_remaining_av) {
					GetPlayerData()->voiceRecoveryTime = 0.0f;
				}
				previous_shout = shout;
				previous_shout_time = GetTimeOfShout();
			});
		}
	}
}

void SpellCastEventHandler::HandleShout(RE::TESObjectREFR* caster, RE::SpellItem* casted_power)
{
	RE::Actor* player = caster->As<RE::Actor>();
	RE::TESShout* shout = player->GetCurrentShout();
	if (shout) {
		AsyncSetupRecover(shout);
	}
}

std::string SpellCastEventHandler::CheckEditorID(std::string& editor_id)
{
	RE::TESForm* given_form = RE::TESForm::LookupByEditorID(editor_id);
	if (given_form) {
		return std::string(given_form->GetName());
	}
	return editor_id;
}

std::vector<std::string> SpellCastEventHandler::GetValues(std::string& values) {
	char delimiter = ',';
	std::vector<std::string> words;
	std::string token;
	std::stringstream s_stream(values);
	while (std::getline(s_stream, token, delimiter)) {
		token.erase(remove_if(token.begin(), token.end(), isspace), token.end());
		for (auto& character : token) {
			character = tolower((unsigned char)character);
		}
		words.push_back(token);
	}
	return words;
}

void SpellCastEventHandler::ImportFile(std::string& file_path, std::map<std::string, RE::ActorValue>& actor_value_mapping)
{
	std::lock_guard<std::shared_mutex> lk(settings_mtx);
	CSimpleIniA ini;
	ini.SetUnicode();
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::wstring converted_file_path = converter.from_bytes(file_path);
	const wchar_t* w_file_path = converted_file_path.c_str();
	ini.LoadFile(w_file_path);

	std::list<CSimpleIniA::Entry> shout_mappings;
	ini.GetAllKeys("Mappings", shout_mappings);

	for (auto& entry : shout_mappings) {
		auto editor_id = std::string(entry.pItem);
		const char* c_editor_id = editor_id.data();
		auto values = ini.GetValue("Mappings", c_editor_id, "");
		std::string s_values(values);
		auto split_values = GetValues(s_values);
		editor_id = CheckEditorID(editor_id);
		if (!shout_values.contains(editor_id)) {
			std::vector<RE::ActorValue> new_values;
			shout_values[editor_id] = new_values;
			for (auto& split_value : split_values) {
				if (actor_value_mapping.contains(split_value)) {
					shout_values[editor_id].push_back(actor_value_mapping[split_value]);
				}
			}
		}
	}
}

void SpellCastEventHandler::ImportSettings()
{
	std::string directory_path = "Data\\SKSE\\Plugins\\Dragontongue";
	std::vector<std::string> file_paths;
	for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
		std::string file_path = entry.path().string();
		file_paths.push_back(file_path);
	}

	std::map<std::string, RE::ActorValue> actor_value_mapping = { { "health", RE::ActorValue::kHealth },
		{ "magicka", RE::ActorValue::kMagicka }, { "stamina", RE::ActorValue::kStamina } };

	std::string base_file = "Data\\SKSE\\Plugins\\Dragontongue.ini";
	ImportFile(base_file, actor_value_mapping);
	for (auto& file_path : file_paths) {
		ImportFile(file_path, actor_value_mapping);
	}
}