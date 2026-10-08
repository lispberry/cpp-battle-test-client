#!/usr/bin/env python3
"""Checks a battle log against the rules of docs/SPEC.md, independently of the engine.

    python3 tests/e2e/spec_check.py <scenario.txt> <log.txt>

The checker replays the log on its own model of the game. Random choices (which neighbour is hit, whether an ability
fires) are not predicted; every logged choice must be one of the legal ones and every number must follow the rules:

- units act in creation order, one action per turn; dead units no longer act and free their cell at once;
- a move goes to a free neighbouring cell (of 8) and brings the unit closer to its march target; a unit whose next cell
  is taken waits; MARCH to the unit's own cell ends with MARCH_ENDED on its first idle turn (as in the prototype);
- swordsman: hits a random adjacent unit for Strength, or with Chance/1000 uses rending instead (Rending damage);
  moves only when nobody is adjacent;
- hunter: with nobody adjacent, shoots a unit 2..Range cells away for Agility, or with Chance/1000 uses poison arrows
  instead; with someone adjacent, stabs for Strength; moves only when it has no target;
- poison deals Poison in total over 5 rounds (split evenly, the first `Poison % 5` rounds one more), at the end of each
  round starting with the one it was applied in, as damage of the hunter; a round's portion is doubled if the poisoned
  unit takes rending in that round;
- one UNIT_ATTACKED line per attacker and target in an action, with the summed damage and the hp left;
- the battle ends when no unit can act and no poison is left (and not before).

Implementation choices the spec leaves open are checked as implemented: a turn is a round of the log, the march step
is diagonal-first (as in the prototype), poison ticks at round end.
"""

import re
import sys

CHANCE_MAX = 1000
POISON_ROUNDS = 5


class Violation(Exception):
    pass


def fail(where, message):
    raise Violation(f"{where}: {message}")


def chebyshev(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def sign(value):
    return (value > 0) - (value < 0)


class Unit:
    def __init__(self, kind, uid, x, y, hp, params):
        self.kind = kind
        self.id = uid
        self.pos = (x, y)
        self.hp = hp
        self.p = params
        self.target = None
        self.alive = True
        self.poisons = []  # [source, portions, amplified_round]


class Game:
    def __init__(self):
        self.width = self.height = None
        self.units = []  # creation order
        self.by_id = {}

    def living(self):
        return [u for u in self.units if u.alive]

    def occupant(self, pos):
        return next((u for u in self.living() if u.pos == pos), None)

    def adjacent(self, unit):
        return [o for o in self.living() if o is not unit and chebyshev(o.pos, unit.pos) == 1]

    def in_range(self, unit):
        return [o for o in self.living() if o is not unit and 2 <= chebyshev(o.pos, unit.pos) <= unit.p["range"]]

    def next_step(self, unit):
        if unit.target is None or unit.pos == unit.target:
            return None  # no order, or one to the cell the unit stands on (it only reports MARCH_ENDED)
        return (unit.pos[0] + sign(unit.target[0] - unit.pos[0]), unit.pos[1] + sign(unit.target[1] - unit.pos[1]))

    def can_act(self, unit):
        if self.adjacent(unit):
            return True
        if unit.kind == "hunter" and self.in_range(unit):
            return True
        if unit.target is not None and unit.pos == unit.target:
            return True
        step = self.next_step(unit)
        return step is not None and self.occupant(step) is None


SPAWN_FIELDS = {
    "SPAWN_SWORDSMAN": ("swordsman", ["strength", "chance", "rending"]),
    "SPAWN_HUNTER": ("hunter", ["agility", "strength", "range", "chance", "poison"]),
}


def read_scenario(path):
    game = Game()
    expected_setup = []
    for raw in open(path, encoding="utf-8"):
        tokens = raw.split()
        if not tokens or tokens[0].startswith("//"):
            continue
        name, args = tokens[0], [int(t) for t in tokens[1:]]
        if name == "CREATE_MAP":
            game.width, game.height = args
            expected_setup.append(f"MAP_CREATED width={args[0]} height={args[1]}")
        elif name in SPAWN_FIELDS:
            kind, fields = SPAWN_FIELDS[name]
            uid, x, y, hp = args[:4]
            unit = Unit(kind, uid, x, y, hp, dict(zip(fields, args[4:])))
            game.units.append(unit)
            game.by_id[uid] = unit
            expected_setup.append(f"UNIT_SPAWNED unitId={uid} unitType={kind} x={x} y={y}")
        elif name == "MARCH":
            uid, tx, ty = args
            unit = game.by_id[uid]
            unit.target = (tx, ty)
            expected_setup.append(
                    f"MARCH_STARTED unitId={uid} x={unit.pos[0]} y={unit.pos[1]} targetX={tx} targetY={ty}")
        else:
            raise ValueError(f"unknown command {name}")
    return game, expected_setup


LINE = re.compile(r"^\[(\d+)\] ([A-Z_]+) (.*)$")


def read_log(path):
    rounds = {}
    for number, raw in enumerate(open(path, encoding="utf-8"), 1):
        raw = raw.rstrip("\n")
        match = LINE.match(raw)
        if not match or not raw.endswith(" "):
            fail(f"log line {number}", f"not in the '[tick] NAME key=value ' format: {raw!r}")
        tick, name, rest = int(match[1]), match[2], match[3]
        fields = dict(pair.split("=", 1) for pair in rest.split())
        rounds.setdefault(tick, []).append((number, name, fields))
    return rounds


class Round:
    def __init__(self, game, number, lines):
        self.game = game
        self.number = number
        self.lines = lines
        self.index = 0

    def peek(self):
        return self.lines[self.index] if self.index < len(self.lines) else None

    def take(self, name, where):
        line = self.peek()
        if line is None or line[1] != name:
            got = "end of round" if line is None else f"line {line[0]} {line[1]} {line[2]}"
            fail(where, f"expected {name}, got {got}")
        self.index += 1
        return line

    def damage(self, attacker_id, target, amount, where, line):
        number, _, f = line
        if int(f["attackerUnitId"]) != attacker_id or int(f["targetUnitId"]) != target.id:
            fail(where, f"line {number}: expected attacker {attacker_id} -> target {target.id}, got {f}")
        if int(f["damage"]) != amount:
            fail(where, f"line {number}: damage {f['damage']}, the rules give {amount}")
        target.hp = max(0, target.hp - amount)
        if int(f["targetHp"]) != target.hp:
            fail(where, f"line {number}: targetHp {f['targetHp']}, expected {target.hp}")

    def bury(self, killed, where):
        for unit in sorted(killed, key=self.game.units.index):
            _, _, f = self.take("UNIT_DIED", where)
            if int(f["unitId"]) != unit.id:
                fail(where, f"UNIT_DIED unitId={f['unitId']}, expected {unit.id}")
            unit.alive = False

    def hit(self, attacker, candidates, normal, ability, where):
        """One attack turn: a plain hit for `normal`, or `ability` = (name, damage or None for poison)."""
        line = self.peek()
        if line is None or line[1] not in ("UNIT_ATTACKED", "UNIT_ABILITY_USED"):
            fail(where, f"has targets {[c.id for c in candidates]} but does not attack (got {line})")
        chance = attacker.p.get("chance", 0)
        if line[1] == "UNIT_ABILITY_USED":
            if ability is None or chance == 0:
                fail(where, f"line {line[0]}: uses an ability it cannot use here (chance {chance})")
            self.index += 1
            if line[2] != {"abilityUnitId": str(attacker.id), "abilityName": ability[0]}:
                fail(where, f"line {line[0]}: expected ability {ability[0]} of {attacker.id}, got {line[2]}")
            if ability[1] is None:
                return "poison"
            amount = ability[1]
        else:
            if ability is not None and chance >= CHANCE_MAX:
                fail(where, f"line {line[0]}: chance {chance}/1000 must always use {ability[0]}")
            amount = normal
        attacked = self.take("UNIT_ATTACKED", where)
        target = self.game.by_id.get(int(attacked[2]["targetUnitId"]))
        if target not in candidates:
            fail(where, f"line {attacked[0]}: target {attacked[2]['targetUnitId']} is not a legal target "
                        f"(legal: {[c.id for c in candidates]})")
        self.damage(attacker.id, target, amount, where, attacked)
        if ability is not None and ability[0] == "rending" and line[1] == "UNIT_ABILITY_USED":
            for poison in target.poisons:
                poison[2] = self.number
            self.rended.setdefault(target.id, []).append(self.turn_index)
        if target.hp == 0:
            self.bury([target], where)
        return None

    def march(self, unit, where):
        if unit.target is not None and unit.pos == unit.target:
            # Ordered to its own cell (as in the prototype): the march ends on the unit's first idle turn.
            _, _, f = self.take("MARCH_ENDED", where)
            if f != {"unitId": str(unit.id), "x": str(unit.pos[0]), "y": str(unit.pos[1])}:
                fail(where, f"bad MARCH_ENDED {f}")
            unit.target = None
            return
        step = self.game.next_step(unit)
        if step is None:
            return
        if self.game.occupant(step) is not None:
            return  # blocked: waits
        _, _, f = self.take("UNIT_MOVED", where)
        moved = (int(f["x"]), int(f["y"]))
        if int(f["unitId"]) != unit.id:
            fail(where, f"UNIT_MOVED of {f['unitId']}, expected {unit.id}")
        if not (0 <= moved[0] < self.game.width and 0 <= moved[1] < self.game.height):
            fail(where, f"moved off the map to {moved}")
        if chebyshev(moved, unit.pos) != 1 or self.game.occupant(moved) is not None:
            fail(where, f"moved from {unit.pos} to {moved}: not a free neighbouring cell")
        if chebyshev(moved, unit.target) >= chebyshev(unit.pos, unit.target):
            fail(where, f"moved from {unit.pos} to {moved}: not closer to {unit.target}")
        unit.pos = moved
        if moved == unit.target:
            _, _, f = self.take("MARCH_ENDED", where)
            if f != {"unitId": str(unit.id), "x": str(moved[0]), "y": str(moved[1])}:
                fail(where, f"bad MARCH_ENDED {f}")
            unit.target = None

    def turn(self, unit):
        where = f"round {self.number}, turn of {unit.kind} {unit.id}"
        game = self.game
        adjacent = game.adjacent(unit)
        if unit.kind == "swordsman":
            if adjacent:
                self.hit(unit, adjacent, unit.p["strength"], ("rending", unit.p["rending"]), where)
                return
        else:
            ranged = game.in_range(unit)
            if not adjacent and ranged:
                if self.hit(unit, ranged, unit.p["agility"], ("poison_arrows", None), where) == "poison":
                    # The target is named by the first tick, at the end of this round.
                    pending = [unit.id, split(unit.p["poison"]), self.turn_index, ranged]
                    self.pending_poisons.append(pending)
                return
            if adjacent:
                self.hit(unit, adjacent, unit.p["strength"], None, where)
                return
        self.march(unit, where)

    def round_end(self):
        where = f"round {self.number}, round end"
        game = self.game
        actual = [(int(f["attackerUnitId"]), int(f["targetUnitId"]), int(f["damage"]))
                  for _, name, f in self.lines[self.index:] if name == "UNIT_ATTACKED"]

        # The log does not name a poisoned arrow's target; its first tick at round end does. Try every unit the hunter
        # could have shot (or none, if they died this round) and keep the assignment that explains the log.
        dying = {int(f["unitId"]) for _, name, f in self.lines[self.index:] if name == "UNIT_DIED"}
        options = []
        for source, portions, shot_at, candidates in self.pending_poisons:
            # A tick names the target, unless an earlier tick this round end killed it.
            choices = [t for t in candidates
                       if t.alive and (t.id in dying or any(a == source and t.id == d for a, d, _ in actual))]
            if sum(portions) == 0 or any(not t.alive for t in candidates):
                choices.append(None)
            options.append(choices)

        expected, poisons = self.assign(options, actual)
        if expected is None:
            fail(where, f"no poison targets explain the round end damage {actual}")

        for unit in game.living():
            unit.poisons = [p for p in poisons[unit.id] if p[1]]
        killed = []
        for attacker, target, total in expected:
            line = self.take("UNIT_ATTACKED", where)
            self.damage(attacker, target, total, where, line)
            if target.hp == 0 and target not in killed:
                killed.append(target)
        self.bury(killed, where)
        if self.peek() is not None:
            fail(where, f"unexpected line {self.peek()}")

    def assign(self, options, actual):
        """Backtracking over the targets of this round's poisoned arrows. A unit is checked as soon as no remaining
        arrow can still reach it, so a wrong choice is dropped early instead of trying every combination."""
        game = self.game
        lines_of = {}
        for attacker, target, damage in actual:
            lines_of.setdefault(target, []).append((attacker, damage))
        reach_from = [set() for _ in options]
        for i in range(len(options) - 1, -1, -1):
            reach_from[i] = (reach_from[i + 1] if i + 1 < len(options) else set()) | {
                    t.id for t in options[i] if t is not None}

        def with_new(chosen):
            poisons = {u.id: [[p[0], list(p[1]), p[2], p[3]] for p in u.poisons] for u in game.living()}
            for (source, portions, shot_at, _), target in zip(self.pending_poisons, chosen):
                if target is not None:
                    # Rending later in this round, after the poison landed, doubles this round's portion.
                    amplified = self.number if any(t > shot_at for t in self.rended.get(target.id, [])) else None
                    poisons[target.id].append([source, list(portions), amplified, self.number])
            return poisons

        def unit_matches(unit, poisons):
            ticks = self.ticks({unit.id: poisons[unit.id]}, [unit])
            return [(a, d) for a, _, d in ticks] == lines_of.get(unit.id, [])

        def search(i, chosen):
            if i == len(options):
                poisons = with_new(chosen)
                expected = self.ticks(poisons, game.living())
                if [(a, t.id, d) for a, t, d in expected] == actual:
                    return expected, poisons
                return None
            later = reach_from[i + 1] if i + 1 < len(options) else set()
            for target in options[i]:
                chosen.append(target)
                closed = [] if target is None or target.id in later else [target]
                poisons = with_new(chosen) if closed else None
                if all(unit_matches(unit, poisons) for unit in closed):
                    found = search(i + 1, chosen)
                    if found:
                        return found
                chosen.pop()
            return None

        return search(0, []) or (None, None)

    def ticks(self, poisons, units):
        """The round end damage, as [attacker, target, total] in log order; spends one portion of every poison."""
        expected = []
        for unit in units:
            hp = unit.hp
            for poison in poisons[unit.id]:
                portion = poison[1].pop(0)
                if poison[2] == self.number:
                    portion *= 2
                if portion > 0 and hp > 0:  # portions after the killing one do not land
                    hp = max(0, hp - portion)
                    for tally in expected:
                        if tally[0] == poison[0] and tally[1] is unit:
                            tally[2] += portion
                            break
                    else:
                        expected.append([poison[0], unit, portion])
        return expected

    def run(self):
        self.pending_poisons = []
        self.rended = {}
        for self.turn_index, unit in enumerate(list(self.game.units)):
            if unit.alive:
                self.turn(unit)
        self.round_end()


def split(total):
    base, rest = divmod(total, POISON_ROUNDS)
    return [base + 1 if i < rest else base for i in range(POISON_ROUNDS)]


def check(scenario_path, log_path):
    game, expected_setup = read_scenario(scenario_path)
    rounds = read_log(log_path)
    setup = [f"{name} " + " ".join(f"{k}={v}" for k, v in fields.items()) for _, name, fields in rounds.pop(0, [])]
    if setup != expected_setup:
        fail("round 0", f"setup lines differ from the scenario:\n  log:      {setup}\n  scenario: {expected_setup}")

    last = max(rounds, default=0)
    for number in range(1, last + 1):
        Round(game, number, rounds.get(number, [])).run()

    # The battle must be over: nobody can act and no poison is left.
    for unit in game.living():
        if game.can_act(unit):
            fail(f"after round {last}", f"the log ends but {unit.kind} {unit.id} can still act")
        if any(sum(poison[1]) > 0 for poison in unit.poisons):
            fail(f"after round {last}", f"the log ends but {unit.kind} {unit.id} is still poisoned")
    return last


def main():
    if len(sys.argv) != 3:
        print(__doc__.splitlines()[2].strip(), file=sys.stderr)
        return 2
    try:
        rounds = check(sys.argv[1], sys.argv[2])
    except Violation as violation:
        print(f"SPEC VIOLATION in {sys.argv[2]}: {violation}", file=sys.stderr)
        return 1
    print(f"ok: {rounds} rounds follow the spec")
    return 0


if __name__ == "__main__":
    sys.exit(main())
