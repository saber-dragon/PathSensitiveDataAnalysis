# Demo: CFG Modification


## How is The Modification Looks Like?

### Demo 01 : Single Block (No Successor)

Original CFG:
![](./01/cfg01_before_modify.png)

After Modification:
![](./01/cfg01_modified.png)


### Demo 02 : Single Block with Single Successors

Original CFG:
![](./02/cfg02_before_modify.png)

After Modification:
![](./02/cfg02_modified.png)

## Usage

```bash
cd /path/to/the/corresponding/demo/directory
make clean
make
```

Make sure that you have used the correct version of LLVM `opt` (should be 4.0.1). And make sure that you have installed graphviz.