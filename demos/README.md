# Demo: CFG Modification


## How is The Modification Looks Like?

### Demo 01 : Single Block (No Successor)

Original CFG:
![](./final/demo01.png)

After Modification:
![](./final/demo01_modified.png)


### Demo 02 : Single Block with Single Successors

Original CFG:
![](./final/demo02.png)

After Modification:
![](./final/demo02_modified.png)

### Demo 3 : A Region of Blocks
Original CFG:
![](./final/demo03.png)

After Modification:
![](./final/demo03_modified.png)


## Usage

```bash
cd /path/to/the/corresponding/demo/directory
make clean
make
```

Make sure that you have used the correct version of LLVM `opt` (should be 4.0.1). And make sure that you have installed graphviz.