#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct GymExercise {
  std::string id;
  std::string name;
  int defaultSets = 3;
  int defaultReps = 10;
  float defaultWeight = 50.0f;
};

struct GymRoutine {
  std::string id;
  std::string name;
  std::vector<GymExercise> exercises;
};

struct GymSetRecord {
  int setNumber = 1;
  float weight = 0.0f;
  int reps = 0;
  bool completed = false;
};

struct GymExerciseSession {
  std::string exerciseId;
  std::string exerciseName;
  std::vector<GymSetRecord> sets;
};

struct GymWorkoutSession {
  std::string routineId;
  std::string routineName;
  uint32_t timestamp = 0;
  std::string dateStr;
  std::vector<GymExerciseSession> exercises;
};

class GymTrackerStore {
  static GymTrackerStore instance;

  std::vector<GymRoutine> routines;
  std::vector<GymWorkoutSession> history;
  GymWorkoutSession activeSession;
  bool routinesLoaded = false;
  bool historyLoaded = false;

  void createDefaultRoutines();
  std::string getRoutinesFilePath() const;
  std::string getHistoryFilePath() const;

 public:
  ~GymTrackerStore() = default;

  static GymTrackerStore& getInstance() { return instance; }

  bool loadRoutines();
  bool saveRoutines();

  bool loadHistory();
  bool saveHistory();

  const std::vector<GymRoutine>& getRoutines();
  const GymRoutine* findRoutine(const std::string& routineId);

  // Active workout session methods
  GymWorkoutSession& getActiveSession() { return activeSession; }
  void startRoutineSession(const std::string& routineId, const std::string& routineName);
  GymExerciseSession* getOrCreateExerciseSession(const std::string& exerciseId, const std::string& exerciseName);
  
  // Set recording
  void logSet(const std::string& exerciseId, const std::string& exerciseName, int setNumber, float weight, int reps);
  
  // Query previous performance for progressive overload
  bool getLastLoggedSet(const std::string& exerciseId, float& lastWeight, int& lastReps) const;
  
  // Check if an exercise has completed sets today
  int getCompletedSetsCount(const std::string& exerciseId) const;
  bool isExerciseCompleted(const std::string& exerciseId, int targetSets) const;

  // History access
  const std::vector<GymWorkoutSession>& getHistory();
  int getHistoryCount() const { return static_cast<int>(history.size()); }
};

#define GYM_TRACKER GymTrackerStore::getInstance()
