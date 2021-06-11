#include "pch.h"
#include "freeflight.h"


BAKKESMOD_PLUGIN(freeflight, "Free Flight", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

int velocity = 2500;
bool rotateMode = false;
Rotator lastRotation = { 0, 0, 0 };
Vector lastLocation = { 0, 0, 0 };
bool takeBall = false;

void freeflight::onLoad()
{
	_globalCvarManager = cvarManager;
	
	cvarManager->registerCvar("flight_enabled", "0", "Enable flight mode", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string, CVarWrapper cvar) {
			if (cvar.getBoolValue()) {
				hookEvents();
			}
			else {
				unhookEvents();
			}});

	cvarManager->registerCvar("flight_speed", "2500", "flight speed", true, true, 0)
		.addOnValueChanged([this](std::string, CVarWrapper cvar) {
			velocity = cvar.getIntValue();
			});

	cvarManager->registerCvar("flight_rotation_mode", "0", "flight rotation mode", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string, CVarWrapper cvar) {
			rotateMode = cvar.getBoolValue();
			});

	cvarManager->registerNotifier("flight_rotation_default", [&](std::vector<std::string> args) {
		lastRotation = { 0, 0, 0 };
		}, "", 0);

	cvarManager->registerCvar("flight_take_ball", "0", "should the plugin also move the ball", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string, CVarWrapper cvar) {
			takeBall = cvar.getBoolValue();
			});
}

void freeflight::onUnload()
{
}

// hooks events to allow the plugin to work
void freeflight::hookEvents() {
	auto sw = getSW();

	if (sw.IsNull()) return;

	auto cars = sw.GetCars();

	if (cars.IsNull()) return;

	CarWrapper car = gameWrapper->GetLocalCar();

	if (!car || !car.GetCollisionComponent()) {
		return;
	}

	lastRotation = car.GetRotation();
	lastLocation = car.GetLocation();

	gameWrapper->HookEventPost("Function TAGame.Car_TA.SetVehicleInput",
		[this](...) { onTick(); });
}

// hooks events to allow the plugin to work
void freeflight::unhookEvents() {
	gameWrapper->UnhookEventPost("Function TAGame.Car_TA.SetVehicleInput");
}

ServerWrapper freeflight::getSW() {
	if (gameWrapper->IsInOnlineGame()) {
		return NULL;
	}
	else if (gameWrapper->IsInGame()) {
		return  gameWrapper->GetGameEventAsServer();
	}
	//cvarManager->log("no server");
	return NULL;
}

void freeflight::onTick() {
	auto sw = getSW();

	if (sw.IsNull()) return;

	auto cars = sw.GetCars();

	if (cars.IsNull()) return;

	CarWrapper car = gameWrapper->GetLocalCar();

	if (!car || !car.GetCollisionComponent()) {
		return;
	}

	PlayerControllerWrapper controller = car.GetPlayerController();

	if (!controller) {
		return;
	}

	ControllerInput input = controller.GetVehicleInput();

	/*cvarManager->log("pitch: " + std::to_string(input.Pitch));
	cvarManager->log("steer: " + std::to_string(input.Steer));
	cvarManager->log("throttle: " + std::to_string(input.Throttle));*/

	Vector velocityInput = {
		-1 * input.Pitch * velocity,
		input.Steer * velocity,
		input.Throttle * velocity
	};

	CameraWrapper camera = gameWrapper->GetCamera();

	if (!camera) {
		return;
	}

	if (rotateMode) {
		car.SetLocation(lastLocation);
		car.SetVelocity({ 0, 0, 0 });
		lastRotation = car.GetRotation();
	}
	else {
		//car.SetVelocity(velocityInput);
		car.SetRotation(lastRotation);
		lastLocation = car.GetLocation();

		if (camera.GetCameraState() == "CameraState_BallCam_TA") {
			car.SetVelocity(RotateVectorWithQuat(velocityInput, RotatorToQuat(camera.GetRotation())));
		}
		else {
			car.SetVelocity(RotateVectorWithQuat(velocityInput, RotatorToQuat(lastRotation)));
		}
	}

	if (takeBall) {
		BallWrapper ball = sw.GetBall();

		if (!ball) {
			return;
		}

		ball.SetLocation(car.GetLocation() + RotateVectorWithQuat({ 400, 0, 0 }, RotatorToQuat(car.GetRotation())));
		
		if (camera.GetCameraState() == "CameraState_BallCam_TA") {
			ball.SetVelocity(RotateVectorWithQuat(velocityInput, RotatorToQuat(camera.GetRotation())));
		} else {
			ball.SetVelocity(RotateVectorWithQuat(velocityInput, RotatorToQuat(lastRotation)));
		}
	}
}

std::string freeflight::GetPluginName() {
	return "Free Flight";
}

void freeflight::SetImGuiContext(uintptr_t ctx) {
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

void freeflight::RenderSettings() {
	CVarWrapper enableCvar = cvarManager->getCvar("flight_enabled");
	
	if (!enableCvar) {
		return;
	}

	bool enabled = enableCvar.getBoolValue();

	if (ImGui::Checkbox("Enable plugin", &enabled)) {
		gameWrapper->Execute([this, enableCvar, enabled](...) mutable {
			enableCvar.setValue(enabled);
			});
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Toggle flight");
	}

	

	ImGui::TextUnformatted("Get free flight in Rocket League");
	ImGui::TextUnformatted("Note that the camera isn't great right now and it's a bit hard to control");
	ImGui::TextUnformatted("Doesn't really work with keyboard and mouse");

	ImGui::NewLine();

	ImGui::TextUnformatted("While in normal cam, stick up will move you in the direction the car faces, and down in the opposite");
	ImGui::TextUnformatted("Left and right will move you in those directions relative to the car's front");
	
	ImGui::NewLine();

	ImGui::TextUnformatted("While in ball cam, stick up will move you in the direction the camera faces, and down in the opposite");
	ImGui::TextUnformatted("Left will move you left relative to the camera. If looking at the ball this ends up being clockwise around it");
	ImGui::TextUnformatted("Right is counterclockwise");

	ImGui::NewLine();

	ImGui::TextUnformatted("In both cameras, throttle up moves your car up");
	ImGui::TextUnformatted("Throttle down moves your car down");

	ImGui::NewLine();

	ImGui::TextUnformatted("Taking the ball puts it a bit in front of your car no matter its rotation");

	CVarWrapper takeBallCvar = cvarManager->getCvar("flight_take_ball");

	if (!takeBallCvar) {
		return;
	}

	bool takeBallEnable = takeBallCvar.getBoolValue();

	if (ImGui::Checkbox("Take the ball", &takeBallEnable)) {
		takeBallCvar.setValue(takeBallEnable);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Takes the ball with you");
	}

	ImGui::TextUnformatted("Rotation mode allows you to rotate your car normally while it's held in place");

	CVarWrapper rotationCvar = cvarManager->getCvar("flight_rotation_mode");
	bool rotation = rotationCvar.getBoolValue();

	if (!rotationCvar) {
		return;
	}

	if (ImGui::Checkbox("Enable rotation mode. When enabled you sit in place and can rotate", &rotation)) {
		rotationCvar.setValue(rotation);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Toggle rotation");
	}

	ImGui::SameLine();

	if (ImGui::Button("Default Rotation")) {
		cvarManager->executeCommand("flight_rotation_default");
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Return to default rotation");
	}

	CVarWrapper velocityCvar = cvarManager->getCvar("flight_speed");

	int newVelocity = velocity;
	if (ImGui::SliderInt("Max Car Speed", &newVelocity, 0, 2500)) {
		velocityCvar.setValue(newVelocity);
	}

	ImGui::Separator();

	ImGui::TextUnformatted("Plugin made by JerryTheBee#1117 - DM me on discord for custom plugin commissions!");
}