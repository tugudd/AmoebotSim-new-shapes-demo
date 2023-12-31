/* Copyright (C) 2021 Joshua J. Daymude, Robert Gmyr, and Kristian Hinnenthal.
 * The full GNU GPLv3 can be found in the LICENSE file, and the full copyright
 * notice can be found at the top of main/main.cpp. */

#include "alg/shapeformation.h"

#include <QtGlobal>
#include <iostream>

ShapeFormationParticle::ShapeFormationParticle(const Node head,
                                               const int globalTailDir,
                                               const int orientation,
                                               AmoebotSystem& system,
                                               State state, const QString mode,
                                               int length, int currentLen, int flag)
  : AmoebotParticle(head, globalTailDir, orientation, system),
    state(state),
    mode(mode),
    length(length),
    currentLen(currentLen),
    flag(flag),
    constructionDir(-1),
    moveDir(-1),
    followDir(-1) {
  if (state == State::Seed) {
    constructionDir = 0;
  }
}

void ShapeFormationParticle::activate() {
  if (isExpanded()) {
    if (state == State::Follow) {
      if (!hasNbrInState({State::Idle}) && !hasTailFollower()) {
        contractTail();
      }
      return;
    } else if (state == State::Lead) {
      if (!hasNbrInState({State::Idle}) && !hasTailFollower()) {
        contractTail();
        updateMoveDir();
      }
      return;
    } else {
      Q_ASSERT(false);
    }
  } else {
    if (state == State::Seed) {
      return;
    } else if (state == State::Idle) {
      if (hasNbrInState({State::Seed, State::Finish})) {
        state = State::Lead;
        updateMoveDir();
        return;
      } else if (hasNbrInState({State::Lead, State::Follow})) {
        state = State::Follow;
        followDir = labelOfFirstNbrInState({State::Lead, State::Follow});
        return;
      }
    } else if (state == State::Follow) {
      if (hasNbrInState({State::Seed, State::Finish})) {
        state = State::Lead;
        updateMoveDir();
        return;
      } else if ((mode == "hh" || mode == "ht") && isLocked()) {
        state = State::Locked;
        return;
      } else if (hasTailAtLabel(followDir)) {
        auto nbr = nbrAtLabel(followDir);
        int nbrContractionDir = nbrDirToDir(nbr, (nbr.tailDir() + 3) % 6);
        push(followDir);
        followDir = nbrContractionDir;
        return;
      }
    } else if (state == State::Lead) {
      if (canFinish()) {
        state = State::Finish;
        updateConstructionDir();
        return;
      } else {
        if ((mode == "hh" || mode == "ht") && isLocked()) {
          state = State::Locked;
          return;
        }
        updateMoveDir();
        if (!hasNbrAtLabel(moveDir)) {
          expand(moveDir);
        } else if (hasTailAtLabel(moveDir)) {
          push(moveDir);
        }
        return;
      }
    }
  }
}

int ShapeFormationParticle::headMarkColor() const {
  switch(state) {
    case State::Seed:   return 0x00ff00;
    case State::Idle:   return -1;
    case State::Follow: return 0x0000ff;
    case State::Lead:   return 0xff0000;
    case State::Finish: return 0x000000;
    case State::Locked: return 0xffff00;
  }

  return -1;
}

int ShapeFormationParticle::headMarkDir() const {
  if (state == State::Seed || state == State::Finish) {
    return constructionDir;
  } else if (state == State::Lead) {
    return moveDir;
  } else if (state == State::Follow) {
    return followDir;
  }

  return -1;
}

int ShapeFormationParticle::tailMarkColor() const {
  return headMarkColor();
}

QString ShapeFormationParticle::inspectionText() const {
  QString text;
  text += "head: (" + QString::number(head.x) + ", " + QString::number(head.y) +
    ")\n";
  text += "orientation: " + QString::number(orientation) + "\n";
  text += "globalTailDir: " + QString::number(globalTailDir) + "\n";
  text += "state: ";
  text += [this](){
    switch(state) {
      case State::Seed:   return "seed";
      case State::Idle:   return "idle";
      case State::Follow: return "follow";
      case State::Lead:   return "lead";
      case State::Finish: return "finish";
      case State::Locked: return "locked";
      default:            return "no state";
    }
  }();
  text += "\n";
  text += "constructionDir: " + QString::number(constructionDir) + "\n";
  text += "moveDir: " + QString::number(moveDir) + "\n";
  text += "followDir: " + QString::number(followDir) + "\n";
  text += "turnSignal: " + QString::number(turnSignal) + "\n";
  text += "shape: ";
  text += [this](){
    if (mode == "h") {
      return "hexagon";
    } else if (mode == "s") {
      return "square";
    } else if (mode == "t1") {
      return "vertex triangle";
    } else if (mode == "t2") {
      return "center triangle";
    } else if (mode == "l") {
      return "line";
    } else if (mode == "z") {
      return "zigzag";
    } else if (mode == "hh") {
      return "hollow hexagon";
    } else if (mode == "ht") {
      return "hollow triangle";
    }
    else {
      return "ERROR";
    }
  }();
  text += "\n";

  return text;
}

ShapeFormationParticle& ShapeFormationParticle::nbrAtLabel(int label) const {
  return AmoebotParticle::nbrAtLabel<ShapeFormationParticle>(label);
}

int ShapeFormationParticle::labelOfFirstNbrInState(
    std::initializer_list<State> states, int startLabel) const {
  auto prop = [&](const ShapeFormationParticle& p) {
    for (auto state : states) {
      if (p.state == state) {
        return true;
      }
    }
    return false;
  };

  return labelOfFirstNbrWithProperty<ShapeFormationParticle>(prop, startLabel);
}

bool ShapeFormationParticle::hasNbrInState(std::initializer_list<State> states)
    const {
  return labelOfFirstNbrInState(states) != -1;
}

int ShapeFormationParticle::constructionReceiveDir() const {
  auto prop = [&](const ShapeFormationParticle& p) {
    if (p.mode == "l") {
      return isContracted() &&
          (p.state == State::Seed || p.state == State::Finish) &&
          (pointsAtMe(p, p.constructionDir) ||
           pointsAtMe(p, (p.constructionDir + 3) % 6));
    } else if (p.mode == "z") {
      return isContracted() &&
             (((p.state == State::Seed || p.state == State::Finish) &&
               pointsAtMe(p, p.constructionDir)) ||
              (p.state == State::Seed && pointsAtMe(p, (p.constructionDir + 1) % 6)));
    } else if (p.mode == "t1b") {
      return isContracted() &&
             (((p.state == State::Seed || p.state == State::Finish) &&
                                    pointsAtMe(p, p.constructionDir)) ||
                                (p.state == State::Seed && pointsAtMe(p, (p.constructionDir + 3) % 6)));
    } else {
      return isContracted() &&
          (p.state == State::Seed || p.state == State::Finish) &&
          pointsAtMe(p, p.constructionDir);
    }
  };

  return labelOfFirstNbrWithProperty<ShapeFormationParticle>(prop);
}

bool ShapeFormationParticle::canFinish() const {
  return constructionReceiveDir() != -1;
}

bool ShapeFormationParticle::isLocked() const {
  int label = 0;
  int leadNum = 0;
  bool isSeed = false;

  while (label < 6) {
    if (hasNbrAtLabel(label)) {
      if (nbrAtLabel(label).state == State::Locked) {
        return true;
      }
      if (mode == "ht") {
        if (nbrAtLabel(label).state == State::Finish) {
          leadNum++;
          auto p = nbrAtLabel(label);
          if (p.hasNbrAtLabel((p.constructionDir + 5) % 6) &&
              p.nbrAtLabel((p.constructionDir + 5) % 6).state == State::Seed) {
              isSeed = true;
          }
        }
      } else if (mode == "hh") {
        if (nbrAtLabel(label).state == State::Seed && hasNbrAtLabel((label + 5) % 6) &&
            nbrAtLabel((label + 5) % 6).state == State::Finish) {
          auto p = nbrAtLabel((label + 5) % 6);
          if (p.hasNbrAtLabel((p.constructionDir + 1) % 6) &&
              p.nbrAtLabel((p.constructionDir + 1) % 6).state == State::Seed) {
              return true;
          }
        }
      }
    }
    label ++;
  }

  return (leadNum > 3) && isSeed;
}

void ShapeFormationParticle::updateConstructionDir() {
  if (mode == "h") {  // Hexagon construction.
    constructionDir = constructionReceiveDir();
    if (nbrAtLabel(constructionDir).state == State::Seed) {
      constructionDir = (constructionDir + 1) % 6;
    } else {
      constructionDir = (constructionDir + 2) % 6;
    }

    if (hasNbrAtLabel(constructionDir) &&
        nbrAtLabel(constructionDir).state == State::Finish) {
      constructionDir = (constructionDir + 1) % 6;
    }
  } else if (mode == "s") {  // Square construction.
    constructionDir = constructionReceiveDir();
    if (nbrAtLabel(constructionDir).state == State::Seed) {
      constructionDir = (constructionDir + 1) % 6;
    } else if (nbrAtLabel(constructionDir).turnSignal == 0) {
      constructionDir = (constructionDir + 2) % 6;
      turnSignal = 1;
    } else if (nbrAtLabel(constructionDir).turnSignal == 1) {
      constructionDir = (constructionDir + 1) % 6;
      turnSignal = 0;
    }

    if (hasNbrAtLabel(constructionDir) &&
       (nbrAtLabel(constructionDir).state == State::Finish ||
        nbrAtLabel(constructionDir).state == State::Seed)) {
      if (turnSignal == 1) {
        turnSignal = 0;
        constructionDir = (constructionDir + 1) % 6;
      } else if (turnSignal == 0) {
        turnSignal = 1;
        constructionDir = (constructionDir + 2) % 6;
      }
    }
  } else if (mode == "t1" || mode == "t1b") {  // Vertex Triangle construction.
    constructionDir = constructionReceiveDir();
    int labelOfFirstNbr = labelOfFirstNbrInState({State::Finish, State::Seed},
                                                 (constructionDir + 5) % 6);
    int labelOfSecondNbr = -1;

    if (labelOfFirstNbr == (constructionDir + 5) % 6) {
      labelOfSecondNbr = labelOfFirstNbrInState({State::Finish},
                                                (constructionDir + 4) % 6);
    } else {
      labelOfFirstNbr = labelOfFirstNbrInState({State::Finish},
                                               (constructionDir + 1) % 6);
      labelOfSecondNbr = labelOfFirstNbrInState({State::Finish},
                                                (constructionDir + 2) % 6);
    }

    if (nbrAtLabel(constructionDir).state == State::Seed) {
      constructionDir = (constructionDir + 5) % 6;
    } else if ((labelOfFirstNbr == (constructionDir + 5) % 6 &&
                labelOfSecondNbr == (constructionDir + 4) % 6) ||
               (labelOfFirstNbr == (constructionDir + 1) % 6 &&
                labelOfSecondNbr == (constructionDir + 2) % 6)) {
      constructionDir = (constructionDir + 3) % 6;
    } else if (labelOfFirstNbr == (constructionDir + 5) % 6) {
      constructionDir = (constructionDir + 2) % 6;
      turnSignal = 1;
    } else if (labelOfFirstNbr == (constructionDir + 1) % 6) {
      constructionDir = (constructionDir + 4) % 6;
      turnSignal = 0;
    } else if (nbrAtLabel(constructionDir).turnSignal == 0) {
      constructionDir = (constructionDir + 5) % 6;
    } else if (nbrAtLabel(constructionDir).turnSignal == 1) {
      constructionDir = (constructionDir + 1) % 6;
    }
  } else if (mode == "t2") {  // Center Triangle construction.
    constructionDir = (constructionReceiveDir() + 1) % 6;

    if (hasNbrAtLabel(constructionDir) &&
        (nbrAtLabel(constructionDir).state == State::Seed ||
         nbrAtLabel(constructionDir).state == State::Finish)) {
      constructionDir = (constructionDir + 2) % 6;
    }
  } else if (mode == "l") {  // Line construction.
    constructionDir = (constructionReceiveDir() + 3) % 6;
  } else if (mode == "z") {
    constructionDir = constructionReceiveDir();

    auto nbr = nbrAtLabel(constructionDir);
    currentLen = nbr.currentLen + 1;

    bool opp = false;
    if (nbr.state == State::Seed && !pointsAtMe(nbr, nbr.constructionDir)) {
      opp = true;
    }
    if (currentLen > length) {
      currentLen = 2;
    }

    if (currentLen == length) {
      flag = opp ? nbr.flag : -nbr.flag;
      constructionDir = (constructionDir + flag) % 6;
      if (constructionDir < 0) {
        constructionDir += 6;
      }
    } else {
      flag = opp ? -nbr.flag : nbr.flag;
      constructionDir = (constructionDir + 3) % 6;
    }
  } else if (mode == "hh") {
    constructionDir = constructionReceiveDir();
    auto nbr = nbrAtLabel(constructionDir);

    if (!nbr.flag) {
      flag = 0;
      while (hasNbrAtLabel(constructionDir) && (nbrAtLabel(constructionDir).state == State::Finish ||
                                                nbrAtLabel(constructionDir).state == State::Seed)) {
        constructionDir = (constructionDir + 1) % 6;
      }

    } else {
      currentLen = nbr.currentLen + 1;
      if (currentLen > length) {
        currentLen = 2;
      }
      if (currentLen == length) { // n
        constructionDir = (constructionDir + 4) % 6; // replace with 2 to show contrary
      } else {
        constructionDir = (constructionDir + 3) % 6;
      }

      if (hasNbrAtLabel(constructionDir) && (nbrAtLabel(constructionDir).state == State::Finish ||
                                             nbrAtLabel(constructionDir).state == State::Seed)) {
        constructionDir = (constructionDir + 5) % 6; // replace with 1 to show contrary
        flag = 0;
      }
    }

  } else if (mode == "ht") {
    constructionDir = constructionReceiveDir();
    auto nbr = nbrAtLabel(constructionDir);

    if (!nbr.flag) {
      flag = 0;

      constructionDir = (constructionDir + 1) % 6;

      if (nbr.constructionDir == (nbr.constructionReceiveDir() + 4) % 6) {
        constructionDir = (constructionDir + 1) % 6;
      } else if (hasNbrAtLabel(constructionDir) &&
                 (nbrAtLabel(constructionDir).state == State::Seed ||
                  nbrAtLabel(constructionDir).state == State::Finish)) {
        constructionDir = (constructionDir + 2) % 6;
      }

      if (hasNbrAtLabel(constructionDir) &&
          (nbrAtLabel(constructionDir).state == State::Seed ||
           nbrAtLabel(constructionDir).state == State::Finish)) {
        constructionDir = (constructionDir + 1) % 6;
      }
    } else {
      currentLen = nbr.currentLen + 1;
      if (currentLen > length) {
        currentLen = 2;
      }
      if (currentLen == length) {
        constructionDir = (constructionDir + 1) % 6; // replace with 5 to show contrary
      } else {
        constructionDir = (constructionDir + 3) % 6;
      }

      if (hasNbrAtLabel(constructionDir) && (nbrAtLabel(constructionDir).state == State::Finish ||
                                             nbrAtLabel(constructionDir).state == State::Seed)) {
        constructionDir = (constructionDir + 1) % 6; // replace with 5 to show contrary
        flag = 0;
      }
    }
  }
    else {
    // This is executing in an invalid mode.
    Q_ASSERT(false);
  }
}

void ShapeFormationParticle::updateMoveDir() {
  moveDir = labelOfFirstNbrInState({State::Seed, State::Finish});
  int label = moveDir;

  while (hasNbrAtLabel(moveDir) && (nbrAtLabel(moveDir).state == State::Seed ||
                                    nbrAtLabel(moveDir).state == State::Finish))
  {
    moveDir = (moveDir + 5) % 6;
  }

  if (mode == "ht") {
    int cnt = 0;
    bool flag = false;

    while (cnt < 6) {
      if (hasNbrAtLabel(label) && (nbrAtLabel(label).state == State::Seed ||
                                     nbrAtLabel(label).state == State::Finish)) {
        auto p = nbrAtLabel(label);
        if (!p.hasNbrAtLabel(p.constructionDir)) {
          flag = true;
        }
      } else if (flag) {
        moveDir = label;
        return;
      }
      label = (label + 5) % 6;
      cnt ++;
    }
  }
}

bool ShapeFormationParticle::hasTailFollower() const {
  auto prop = [&](const ShapeFormationParticle& p) {
    return p.state == State::Follow &&
           pointsAtMyTail(p, p.dirToHeadLabel(p.followDir));
  };

  return labelOfFirstNbrWithProperty<ShapeFormationParticle>(prop) != -1;
}

ShapeFormationSystem::ShapeFormationSystem(int numParticles, double holeProb,
                                           QString mode, int length) {
  Q_ASSERT(mode == "h" || mode == "s" || mode == "t1" || mode == "t2" ||
           mode == "t1b" || mode == "l" || mode == "z" || mode == "hh" ||
           mode == "ht");
  Q_ASSERT(numParticles > 0);
  Q_ASSERT(0 <= holeProb && holeProb <= 1);

  // Insert the seed at (0,0).
  std::set<Node> occupied;
  insert(new ShapeFormationParticle(Node(0, 0), -1, randDir(), *this,
                                    ShapeFormationParticle::State::Seed, mode, length, 1, -1));
  occupied.insert(Node(0, 0));

  std::set<Node> candidates;
  for (int i = 0; i < 6; ++i) {
    candidates.insert(Node(0, 0).nodeInDir(i));
  }

  // Add all other particles.
  int particlesAdded = 1;
  while (particlesAdded < numParticles && !candidates.empty()) {
    // Pick a random candidate node.
    int randIndex = randInt(0, candidates.size());
    Node randCand;
    for (auto cand = candidates.begin(); cand != candidates.end(); ++cand) {
      if (randIndex == 0) {
        randCand = *cand;
        candidates.erase(cand);
        break;
      } else {
        randIndex--;
      }
    }

    // With probability 1 - holeProb, add a new particle at the candidate node.
    if (randBool(1.0 - holeProb)) {
      insert(new ShapeFormationParticle(randCand, -1, randDir(), *this,
                                        ShapeFormationParticle::State::Idle,
                                        mode, length, 0, 1));
      occupied.insert(randCand);
      particlesAdded++;

      // Add new candidates.
      for (int i = 0; i < 6; ++i) {
        if (occupied.find(randCand.nodeInDir(i)) == occupied.end()) {
          candidates.insert(randCand.nodeInDir(i));
        }
      }
    }
  }
}

bool ShapeFormationSystem::hasTerminated() const {
  #ifdef QT_DEBUG
    if (!isConnected(particles)) {
      return true;
    }
  #endif

  for (auto p : particles) {
    auto hp = dynamic_cast<ShapeFormationParticle*>(p);
    if (hp->state != ShapeFormationParticle::State::Seed &&
        hp->state != ShapeFormationParticle::State::Finish &&
        hp->state != ShapeFormationParticle::State::Locked) {
      return false;
    }
  }

  return true;
}

std::set<QString> ShapeFormationSystem::getAcceptedModes() {
  std::set<QString> set = {"h", "t1", "t1b", "t2", "s", "l", "z", "hh", "ht"};
  return set;
}
