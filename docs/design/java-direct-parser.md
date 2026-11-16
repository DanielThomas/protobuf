# Builder-hosted static decoding

## Goal

Generate a static decoder alongside the existing stateful Builder decoder. The happy path must not construct a Builder or ParsingBuilder, even in the interpreter or with escape analysis disabled. This is explicit allocation avoidance, not scalar replacement.

The Parser object delegates to the static entry. Decoded values and builder bookkeeping are method locals. Normal message publication consumes that state. Construct a builder only when stateful continuation is required.

## Acceptance constraints

Use the existing builder generator's parsing loop, field cases, private helpers, bookkeeping, and publication emitters in place. Bind their storage references to locals for static decoding and members for stateful decoding. Do not add a parallel parser generator, relocate the existing algorithms to a new common layer, or accept equivalent generated output as a substitute for a surgical source diff.

The production diff must make the reuse obvious to maintainers. Existing emitter bodies must remain recognizable and in place; every change must be necessary for static storage, publication, or continuation. No unrelated cleanup, convenience abstractions, generic target framework, or speculative scaffolding. A small generator source diff and minimal perturbation of the generated message class are acceptance criteria, not cleanup after correctness. Keep parsing and handoff machinery in Builder. Growth of the generated Builder class is not itself an acceptance concern; JVM method, parameter and class-file limits still apply. If meeting the design requires substantial new generator machinery, reconsider the design before expanding the patch.

The allowed storage change from the earlier direct-message parser is explicit: the static decoder uses locals for field values as well as builder-only state. This keeps per-tag decoding inside Builder without cross-class access to private message or builder fields. Final message publication reuses the existing publication emitter and message constructor path. Shared Builder-runtime delegates can provide privileged unknown-field handling, unknown-field installation and forced-builder flag access without adding helpers or access bridges to each generated message. The runtime message constructor can accept a null Builder for static publication, leaving generated message constructors unchanged. Pre-Java-11 publication bridges are not a reason to construct a builder.

On handoff, retain the already-read tag and leave its payload untouched. A single cold materialization boundary installs complete state directly: fields, presence and mutability bookkeeping, oneof state, retained children, live builders, and unknown fields. A constructor that skips ordinary default setup followed by bounded direct-store helpers is acceptable; a single full-state constructor is not required. Transfer scalar values and existing references without a transport object, capture chain, default initialization followed by overwriting, or expensive copies. Complete installation and child parenting before exposing the builder to the existing decoder. Do not replay input, merge decoded messages, invoke toBuilder, or finalize collections or unknown fields before transfer. Continue through the existing builder case generation and merge loop.

The builder must behave as though it parsed the input throughout. Ordinary singular and repeated fields retain their existing child lifetime boundaries. Message-valued oneof history survives case switches and nested returns. Map entries retain replacement rather than cross-entry merging semantics. Extensions, compatibility fallbacks, and forced-field-builder testing behavior remain supported.

A clean child can return its message. If hidden child history must survive, retain its actual continuation instead. The exact child exchange is to be validated before changing the generator. It must not require an allocated happy-path result wrapper or builder. Do not infer an arbitrary child's complete builder state from its message when hidden history exists.

Failure publication must preserve the existing unfinished message and collection immutability, including a child that fails before returning its published value.

## First experiment

The first step is a controlled generated-source copy, not a production emitter. Reuse the prior experiments' generated fixtures, cold continuation mechanism, independent Java baseline, and correctness gates. Replace the happy-path builder carrier with locals and lower the existing publication body to consume them.

Explicitly distinguish those research copies from an adopted generator change. The prior experimental continuation subtype is only a control; it is not approval to introduce an additional production state representation or weaken direct state installation.

Before allocation measurement:

- Check that the static entry has no unconditional builder construction and that only designated cold paths construct continuation builders.
- Instrument builder constructors in a separate correctness build and assert zero happy-input constructions before warmup, including under the interpreter.
- Run the existing fixture and recursive compatibility gates against independent pristine generated Java. Include inactive oneof descendants, unknown fields, every tested prefix, malformed input, recursion limits, pending payloads, and immutable snapshots.
- Run child message/continuation protocol checks, with explicit state preservation across a later merge.

Then measure uninstrumented generated copies using actual serializer-produced inputs, separately from duplicate/reactivation inputs. Check builders per input after mixed training, with default inlining policy and with escape analysis disabled. Compile both Java 8 and Java 17 targets and inspect synthetic accessors, distinguishing the decode loop from ordinary publication.

Only after the shape is validated should the existing generator emit it. Review the generator diff and compare the generated message class's source, members, helpers, access bridges and bytecode against clean HEAD as implementation proceeds, including wide schemas and Java 8 targets. Report Builder and message-class measurements separately; combined message-family size is not a proxy for message-class perturbation. Preserve existing method-splitting shapes where needed and respect JVM limits. Passing tests and allocation checks do not waive generator source-preservation or message-class preservation requirements.

Subsequent validation includes all Java core and generator tests, throughput, old JVMs, initialization and extension behavior, and JVM method, parameter, and class-file limits.

## Review baseline

The implementation is reviewed against eda222ac5f2b8f826923117460587ccf09922844 on branch java-parser-base. The draft review separates the production change and core regression tests from the supplemental synthetic validation and source accounting.
