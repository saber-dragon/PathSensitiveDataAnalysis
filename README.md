# Path-sensitive Data-flow Analysis

This project implements the path-sensitive data-flow analysis algorithms proposed in [Comprehensive Path-sensitive Data-flow Analysis](https://dl.acm.org/citation.cfm?id=1356066).

## Algorithm Description

In this project, we (Long, Moses, and Kelsey) implements the path-sensitive data-flow analysis based SCCP. As SCCP is done under SSA form, our implementation only works for the SSA form. Under SSA form, the detection of destructive merges becomes very easy. We just need to detect, for each PHI node, whether there is some constant incoming values. If "yes", then it is a destructive merge. Similarly, the detection of influenced nodes and region of influence also becomes easier (than in traditional form). However, the "cloning" of basic blocks become much harder, since we have to handle every PHI node properly. What's worse, we need to insert some PHI nodes on some or all kill edges.

## Preliminary Results

We did some experiments on some very simple examples. The results can be found at [demos](./demos/README.md).


## Authors

+ Long Gong
+ Moses Ike
+ Kelsey Kurzeja



