# Persistence and compatibility

## Persist only durable state

Persist when the value is part of the user's project/session intent.

Examples:
- track settings
- clip properties
- routing
- feature-specific project state

Usually do not persist:
- transient hover state
- temporary caches
- derived values that can be recomputed
- runtime handles

## Adding a field

Define:
- serialized key/name
- type
- default
- missing-field behavior
- invalid-field behavior
- round-trip test

## Compatibility

Before modifying project schema:
- inspect current schema/version strategy;
- avoid reusing a field with changed meaning;
- decide whether migration is needed;
- test older project load when expected;
- keep unknown/new state failure-safe where possible.

## Save/load acceptance

At minimum:
1. set/change feature state;
2. save;
3. close/reload;
4. verify restored behavior;
5. verify undo stack behavior according to project policy;
6. verify missing/default field behavior if applicable.
