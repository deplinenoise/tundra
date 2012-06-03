
Simple source generation
========================

This example demonstrates an easy way to add build rules that generate source files.

The `DefRule` feature is used to set up a source generator action that is then
called from the test program's `Sources` list.

The example also demonstrates marking the `DefRule` as configuration invariant.
This means there will only be one source file generated for each invocation.
These source files are then shared for all configurations.
