#include "GymTrackerStore.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "util/HeaderDateUtils.h"
#include "util/TimeUtils.h"

GymTrackerStore GymTrackerStore::instance;

namespace {
constexpr char GYM_DIR[] = "/gym";
constexpr char ROUTINES_FILE[] = "/gym/routines.json";
constexpr char HISTORY_FILE[] = "/gym/history.json";
}  // namespace

std::string GymTrackerStore::getRoutinesFilePath() const { return ROUTINES_FILE; }

std::string GymTrackerStore::getHistoryFilePath() const { return HISTORY_FILE; }

void GymTrackerStore::createDefaultRoutines() {
  routines.clear();

  // 1. Push (Pecho / Hombro / Tríceps)
  {
    GymRoutine push;
    push.id = "push";
    push.name = "Push (Pecho/Hombro/Tríceps)";
    push.exercises = {
        {"bench_press", "Press Banca Plano", 4, 8, 80.0f},
        {"overhead_press", "Press Militar (OHP)", 3, 10, 45.0f},
        {"incline_db_press", "Press Inclinado Manc.", 3, 10, 26.0f},
        {"triceps_pushdown", "Extensiones Tríceps", 3, 12, 30.0f},
        {"lateral_raises", "Elevaciones Laterales", 4, 15, 10.0f},
    };
    routines.push_back(std::move(push));
  }

  // 2. Pull (Espalda / Bíceps / Deltoides Post.)
  {
    GymRoutine pull;
    pull.id = "pull";
    pull.name = "Pull (Espalda/Bíceps)";
    pull.exercises = {
        {"barbell_row", "Remo con Barra", 4, 8, 70.0f},
        {"lat_pulldown", "Jalón al Pecho", 3, 10, 65.0f},
        {"seated_cable_row", "Remo en Polea Baja", 3, 10, 55.0f},
        {"biceps_curl", "Curl Bíceps Barra", 3, 10, 30.0f},
        {"face_pulls", "Face Pulls Polea", 4, 15, 25.0f},
    };
    routines.push_back(std::move(pull));
  }

  // 3. Legs (Pierna / Core)
  {
    GymRoutine legs;
    legs.id = "legs";
    legs.name = "Legs (Pierna/Glúteo)";
    legs.exercises = {
        {"barbell_squat", "Sentadilla con Barra", 4, 8, 100.0f},
        {"romanian_deadlift", "Peso Muerto Rumano", 3, 10, 90.0f},
        {"leg_press", "Prensa de Piernas", 3, 12, 160.0f},
        {"leg_curl", "Curl Femoral Tumbado", 3, 12, 45.0f},
        {"calf_raises", "Elevación de Talones", 4, 15, 60.0f},
    };
    routines.push_back(std::move(legs));
  }
}

bool GymTrackerStore::loadRoutines() {
  if (routinesLoaded && !routines.empty()) {
    return true;
  }

  Storage.ensureDirectoryExists(GYM_DIR);

  if (!Storage.exists(ROUTINES_FILE)) {
    LOG_INF("GYM", "routines.json not found, creating default PPL routines");
    createDefaultRoutines();
    saveRoutines();
    routinesLoaded = true;
    return true;
  }

  String content = Storage.readFile(ROUTINES_FILE);
  if (content.length() == 0) {
    createDefaultRoutines();
    saveRoutines();
    routinesLoaded = true;
    return true;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, content.c_str());
  if (err) {
    LOG_ERR("GYM", "Failed to parse routines.json: %s", err.c_str());
    createDefaultRoutines();
    routinesLoaded = true;
    return false;
  }

  routines.clear();
  JsonArray routinesArray = doc["routines"].as<JsonArray>();
  for (JsonObject rObj : routinesArray) {
    GymRoutine r;
    r.id = rObj["id"] | "";
    r.name = rObj["name"] | "";
    JsonArray exArray = rObj["exercises"].as<JsonArray>();
    for (JsonObject eObj : exArray) {
      GymExercise ex;
      ex.id = eObj["id"] | "";
      ex.name = eObj["name"] | "";
      ex.defaultSets = eObj["defaultSets"] | 3;
      ex.defaultReps = eObj["defaultReps"] | 10;
      ex.defaultWeight = eObj["defaultWeight"] | 50.0f;
      r.exercises.push_back(std::move(ex));
    }
    routines.push_back(std::move(r));
  }

  if (routines.empty()) {
    createDefaultRoutines();
    saveRoutines();
  }

  routinesLoaded = true;
  return true;
}

bool GymTrackerStore::saveRoutines() {
  Storage.ensureDirectoryExists(GYM_DIR);

  JsonDocument doc;
  JsonArray routinesArray = doc["routines"].to<JsonArray>();

  for (const auto& r : routines) {
    JsonObject rObj = routinesArray.add<JsonObject>();
    rObj["id"] = r.id;
    rObj["name"] = r.name;
    JsonArray exArray = rObj["exercises"].to<JsonArray>();
    for (const auto& ex : r.exercises) {
      JsonObject eObj = exArray.add<JsonObject>();
      eObj["id"] = ex.id;
      eObj["name"] = ex.name;
      eObj["defaultSets"] = ex.defaultSets;
      eObj["defaultReps"] = ex.defaultReps;
      eObj["defaultWeight"] = ex.defaultWeight;
    }
  }

  String output;
  serializeJsonPretty(doc, output);
  return Storage.writeFile(ROUTINES_FILE, output);
}

bool GymTrackerStore::loadHistory() {
  if (historyLoaded) {
    return true;
  }

  if (!Storage.exists(HISTORY_FILE)) {
    history.clear();
    historyLoaded = true;
    return true;
  }

  String content = Storage.readFile(HISTORY_FILE);
  if (content.length() == 0) {
    historyLoaded = true;
    return true;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, content.c_str());
  if (err) {
    LOG_ERR("GYM", "Failed to parse history.json: %s", err.c_str());
    historyLoaded = true;
    return false;
  }

  history.clear();
  JsonArray sessionsArray = doc["sessions"].as<JsonArray>();
  for (JsonObject sObj : sessionsArray) {
    GymWorkoutSession session;
    session.routineId = sObj["routineId"] | "";
    session.routineName = sObj["routineName"] | "";
    session.timestamp = sObj["timestamp"] | 0;
    session.dateStr = sObj["date"] | "";

    JsonArray exArray = sObj["exercises"].as<JsonArray>();
    for (JsonObject eObj : exArray) {
      GymExerciseSession exSession;
      exSession.exerciseId = eObj["exerciseId"] | "";
      exSession.exerciseName = eObj["exerciseName"] | "";

      JsonArray setsArray = eObj["sets"].as<JsonArray>();
      for (JsonObject setObj : setsArray) {
        GymSetRecord set;
        set.setNumber = setObj["set"] | 1;
        set.weight = setObj["weight"] | 0.0f;
        set.reps = setObj["reps"] | 0;
        set.completed = setObj["completed"] | true;
        exSession.sets.push_back(set);
      }
      session.exercises.push_back(std::move(exSession));
    }
    history.push_back(std::move(session));
  }

  historyLoaded = true;
  return true;
}

bool GymTrackerStore::saveHistory() {
  Storage.ensureDirectoryExists(GYM_DIR);

  JsonDocument doc;
  JsonArray sessionsArray = doc["sessions"].to<JsonArray>();

  // Only keep the most recent 50 sessions to save RAM and flash storage
  const size_t startIdx = history.size() > 50 ? history.size() - 50 : 0;

  for (size_t i = startIdx; i < history.size(); ++i) {
    const auto& s = history[i];
    JsonObject sObj = sessionsArray.add<JsonObject>();
    sObj["routineId"] = s.routineId;
    sObj["routineName"] = s.routineName;
    sObj["timestamp"] = s.timestamp;
    sObj["date"] = s.dateStr;

    JsonArray exArray = sObj["exercises"].to<JsonArray>();
    for (const auto& ex : s.exercises) {
      JsonObject eObj = exArray.add<JsonObject>();
      eObj["exerciseId"] = ex.exerciseId;
      eObj["exerciseName"] = ex.exerciseName;

      JsonArray setsArray = eObj["sets"].to<JsonArray>();
      for (const auto& set : ex.sets) {
        JsonObject setObj = setsArray.add<JsonObject>();
        setObj["set"] = set.setNumber;
        setObj["weight"] = set.weight;
        setObj["reps"] = set.reps;
        setObj["completed"] = set.completed;
      }
    }
  }

  String output;
  serializeJson(doc, output);
  return Storage.writeFile(HISTORY_FILE, output);
}

const std::vector<GymRoutine>& GymTrackerStore::getRoutines() {
  if (!routinesLoaded) {
    loadRoutines();
  }
  return routines;
}

const GymRoutine* GymTrackerStore::findRoutine(const std::string& routineId) {
  if (!routinesLoaded) {
    loadRoutines();
  }
  for (const auto& r : routines) {
    if (r.id == routineId) {
      return &r;
    }
  }
  return nullptr;
}

void GymTrackerStore::startRoutineSession(const std::string& routineId, const std::string& routineName) {
  loadHistory();

  // If activeSession is for a different routine, start new
  if (activeSession.routineId != routineId) {
    activeSession = GymWorkoutSession();
    activeSession.routineId = routineId;
    activeSession.routineName = routineName;
    const auto dateInfo = HeaderDateUtils::getDisplayDateInfo();
    activeSession.timestamp = dateInfo.timestamp;
    activeSession.dateStr = HeaderDateUtils::getDisplayDateText();
    if (activeSession.dateStr.empty()) {
      activeSession.dateStr = "Today";
    }
  }
}

GymExerciseSession* GymTrackerStore::getOrCreateExerciseSession(const std::string& exerciseId,
                                                               const std::string& exerciseName) {
  for (auto& ex : activeSession.exercises) {
    if (ex.exerciseId == exerciseId) {
      return &ex;
    }
  }
  GymExerciseSession newEx;
  newEx.exerciseId = exerciseId;
  newEx.exerciseName = exerciseName;
  activeSession.exercises.push_back(std::move(newEx));
  return &activeSession.exercises.back();
}

void GymTrackerStore::logSet(const std::string& exerciseId, const std::string& exerciseName, const int setNumber,
                             const float weight, const int reps) {
  GymExerciseSession* exSession = getOrCreateExerciseSession(exerciseId, exerciseName);
  if (!exSession) return;

  // Check if set already exists in active session
  bool found = false;
  for (auto& set : exSession->sets) {
    if (set.setNumber == setNumber) {
      set.weight = weight;
      set.reps = reps;
      set.completed = true;
      found = true;
      break;
    }
  }

  if (!found) {
    GymSetRecord rec;
    rec.setNumber = setNumber;
    rec.weight = weight;
    rec.reps = reps;
    rec.completed = true;
    exSession->sets.push_back(rec);
  }

  // Update or append activeSession to history
  loadHistory();
  bool sessionFound = false;
  for (auto& s : history) {
    if (s.routineId == activeSession.routineId && s.dateStr == activeSession.dateStr) {
      s = activeSession;
      sessionFound = true;
      break;
    }
  }
  if (!sessionFound) {
    history.push_back(activeSession);
  }

  saveHistory();
}

bool GymTrackerStore::getLastLoggedSet(const std::string& exerciseId, float& lastWeight, int& lastReps) const {
  if (history.empty()) {
    return false;
  }

  // Search in reverse from most recent session to oldest
  for (auto sIt = history.rbegin(); sIt != history.rend(); ++sIt) {
    // If this session is the active session today, check if there was a set before today
    for (const auto& ex : sIt->exercises) {
      if (ex.exerciseId == exerciseId && !ex.sets.empty()) {
        const auto& lastSet = ex.sets.back();
        if (lastSet.completed) {
          lastWeight = lastSet.weight;
          lastReps = lastSet.reps;
          return true;
        }
      }
    }
  }

  return false;
}

int GymTrackerStore::getCompletedSetsCount(const std::string& exerciseId) const {
  for (const auto& ex : activeSession.exercises) {
    if (ex.exerciseId == exerciseId) {
      int count = 0;
      for (const auto& set : ex.sets) {
        if (set.completed) count++;
      }
      return count;
    }
  }
  return 0;
}

bool GymTrackerStore::isExerciseCompleted(const std::string& exerciseId, const int targetSets) const {
  return getCompletedSetsCount(exerciseId) >= targetSets;
}

const std::vector<GymWorkoutSession>& GymTrackerStore::getHistory() {
  loadHistory();
  return history;
}
