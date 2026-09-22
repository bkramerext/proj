#!/bin/sh
cat variety.csv | proj a b c --sep=, | proj a b c --sep=,
