# Static parser source baseline

This review is against `eda222ac5f2b8f826923117460587ccf09922844` (Cover Java parsing merge and publication behavior), not an unspecified upstream revision. It covers the cumulative production patch, not just the final outlining change. No performance experiments or handoff changes were made during this review.

The production patch SHA-256 is `18159632b0fe88dac0e35277788f187e0e8b8de7a5ab1781d222092f9437c213`, computed with `git diff eda222ac5f2b8f826923117460587ccf09922844 -- java/core/src/main/java/com/google/protobuf src/google/protobuf/compiler/java/full`. The publishing branch preserves this patch exactly; its supplemental validation uses configurable baseline and JDK paths. Generated sources, binaries and local result files are not included.

## Exact accounting

| Production area | Added | Deleted | Net |
| --- | ---: | ---: | ---: |
| Existing Builder orchestrator, message_builder.cc | 488 | 35 | 453 |
| Field emitter bodies, including oneof_generator.cc | 917 | 380 | 537 |
| Generator headers | 118 | 33 | 85 |
| Existing field-generator default initialization hook | 7 | 0 | 7 |
| Existing Parser delegation in message.cc | 29 | 8 | 21 |
| Runtime support, six Java files | 219 | 20 | 199 |
| Total production | 1,778 | 476 | 1,302 |

Generator production alone is +1,559/-456, net +1,103. Tracked regression tests are separately +547/-2, net +545. Documentation and the manually invoked experiment/validation sources are not included in those counts. The supplemental validation and review material are in a separate commit; they are not part of the core regression-test count.

The Builder orchestrator's net +453 breaks down without overlap:

| Change | Net lines |
| --- | ---: |
| Eligibility predicates and scoped storage bindings/dispatch in Generate | 30 |
| Existing buildPartial and publication shard changes | 28 |
| New argument and parameter emission methods | 47 |
| Raw construction, clean restoration, direct installation and child attachment emission | 84 |
| Existing GenerateBuilderParsingMethods, including entry, loop, failure and continuation boundaries | 222 |
| Existing field-case guards and oneof clear-mask binding | 42 |
| Total | 453 |

These are source-diff counts, not counts of independent decoding algorithms or a proof that the patch is small enough. In particular, the parsing-method emitter grows from 45 to 267 source lines. It is the main orchestration review point; its cost should not be obscured by the fact that the decode switch itself is shared.

## What remains in place

- The existing Builder parsing-method emitter still owns the loop and calls the existing field-case emitters. The ordinary instance decoder remains generated from it; there is no second parser generator.
- Each field class keeps its decoding algorithms and private helper emitters. Local decoding either expands an existing helper body or calls the static oneof child helper emitted in that same class. Repeated-field mutability bodies are shared with their existing helpers.
- Existing buildPartial and field publication emitters own publication. Their bounded shards take existing storage and bookkeeping as parameters; there is no second message publication implementation.
- Existing Builder-member declaration emitters supply members, parse locals and shard parameters. The scoped parse_locals and field_access bindings plus initialize argument distinguish these uses. There is no independent field-storage schema or generic target abstraction.
- Normal Builder default initialization remains in the existing constructor hook. Removing declaration initializers lets the raw continuation constructor skip that work without changing message constructors.
- The original instance merge and parse helpers remain the continuation implementation. Direct installation completes before child attachment and before the pending payload is consumed.

The retained state is still the actual child Builder where hidden history exists. No callback factory, result carrier class, transport array, replay decoder, reflective state restoration, or continuation subtype is introduced by this patch.

## Necessary changes beyond storage-name substitution

The patch is not only member-name threading. The following costs are explicit:

- The static entry needs local defaults/bookkeeping, unknown-field handling, partial-message failure publication, and the cold materialization boundary. The ordinary Builder supplies these implicitly through its object and existing runtime base.
- Per-field clean restoration emits direct assignments and presence/mutability bookkeeping. It cannot simply invoke the normal setters or mergeFrom: that would lose raw string representation, perform collection work, or change child lifetimes. Thirteen concrete field classes implement the restoration hook; oneof case restoration is separate. This handles a published clean child only, not an arbitrary stateful child inferred from its message.
- Oneof message parsing needs a framed message-or-actual-Builder exchange, adoption into the existing SingleFieldBuilder slot, and unfinished-child publication. The static branch is in the existing message-field helper emitter; the normal instance branch remains intact.
- A known closed-enum case switch can retain an inactive message child, while an unknown enum value must not change cases. That is why the existing clear-mask emitter has the local retained-child binding and mixed oneofs retain the full child-slot scan.
- Child attachment and field framing retain method boundaries needed by the validated generated shape. Installation shards remain direct loads/stores; attachment is a separate bounded operation afterward.
- Map fields have a localized source movement: the original member/accessor emission is now behind the field-local GenerateMapBuilderMembers helper so its mutable-storage body can also be emitted for locals. This is not a cross-field common parser layer, but it is more than a textual receiver substitution and accounts for much of map_field.cc's churn.

The primitive oneof static path also simplifies the existing case switch to an empty-case check, since the outer guard transfers case changes before reading their payload. This is a deliberate generated control-flow difference, not a claim that every static case is textually identical to its instance counterpart. The original instance body is preserved by the generated-output gate.

## Runtime surface

- AbstractParser: two protected compatibility hooks and package-local child/map dispatch. Legacy parsers return null without consuming input and use the ordinary Builder fallback.
- GeneratedMessage: null-Builder message construction, privileged shared delegates for Builder-hosted static code, and raw unknown-state construction/child parenting. These avoid per-message helpers and pre-Java-11 access bridges.
- CodedInputStream: group framing counterparts to the existing message framing helpers.
- SingleFieldBuilder: actual-Builder adoption and initial-snapshot restoration policy. Legacy, published and retained modes are distinct; replacements clear the initial-snapshot policy.
- MapFieldBuilder: a constructor sharing the immutable snapshot view rather than copying its entries.
- MapEntryLite: the existing two entry loops use the child exchange without introducing a full-runtime type dependency. Entry boundaries and replacement semantics remain unchanged.

No generated message fields, methods or per-field access bridges are added. Builder growth and runtime wrapper changes are reported separately; neither is disguised as message-class preservation.

## Cleanup made during this review

Five field classes repeated the same static-only forwarding implementation of GenerateBuilderInitializationCode. The existing default hook now invokes the existing field initialization emitter for static-enabled Builders; the specialized primitive/bytes implementations remain specialized. This removes five definitions and five declarations without introducing a new API or moving decoding algorithms.

The restoration hook is now pure virtual instead of having a runtime-fatal default that no concrete field generator uses. A missing implementation is a compile-time failure. One unrelated blank-line deletion was also restored. The combined reduction is 34 net production lines, from +1,336 to +1,302.

## Validation and limits of the review

The generator test was executed after the hook change and passed. The complete Java core/generator check reports 258 passing targets, all cached on the final check. Local logs are not included; the supplemental validation commands are documented in [README.md](README.md).

A frozen-before versus freshly regenerated-after comparison checks every Java source byte, not just selected method signatures:

- 10 primitive/empty/wide fixture sources.
- 20 ordinary-child fixture sources.
- 63 all-shape/retained-child/fallback fixture sources.

All 93 synthetic fixture sources are identical. The deliberately pristine legacy child in the compatibility fixture remains pristine; it is not regenerated with the candidate. This review does not substitute that output comparison for source inspection; it establishes that the source cleanup did not alter the already validated generated shape. Existing Java 8/17 differential, identity, allocation and message-layout evidence therefore remains applicable. Runtime production code is unchanged during this review.

The structural reuse review finds no separate decoder generator or generic target framework. It does not turn +1,302 lines into a trivial change, establish an objective minimum, or grant maintainer approval. The source-size decision is now against the fixed patch and the explicit costs above. Further source changes should preserve this baseline's behavior and be justified individually, rather than replace it with another handoff design.
