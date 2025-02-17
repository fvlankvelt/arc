Abstraction and Reasoning Challenge
===================================

Solution to the ARC challenge by combining a powerful DSL with neural-guided search.

Constructing a program for a task involves making a sequence of choices that determine the program.  It needs to have a way of parsing the input/output (the "abstraction").  Then it needs to determine which components in the graph to operate on (the "filter").  Such a selection may depend on a number of criteria, e.g. color, size or degree of the component.  Finally the transformation to apply must be chosen, with its own set of (potentially dynamic) parameters.

The number of choices to be made only grows when the DSL becomes more powerful.  So the search tree must be explored smart, to reduce the number of programs that are tested and find a solution quickly.  The implementation in this project is to train a neural network to guide the search.


Inference Compilation
---------------------

As we want the neural network - the "guide" - to adapt to a wide range of (input, output) pairs and come up with reasonable suggestions of choices to pursue, it needs to be trained on a very wide selection of pairs with their corresponding transformational program.  The way we do that is by 
* taking (input, output) pairs from the training set
* use the guide to select a program, initialized with the (input, output) pair
* take the output' that was created by the program (in general different from the wanted output) and use that to classify the program

This is reminiscent of reinforcement learning, where the program creates/selects its own training data.  It allows us to use existing training data, without setting up an additional dataset.  We can also gauge progress, by seeing how often the neural network guides to the correct solution (where output' equals output).


Neural Network Architecture
---------------------------

The neural network is a bit peculiar, in that it needs to deal with varying input and output sizes.  While the inputs are encoded as pixels in a grid, the concepts needed to transform it into the correct output are not (always) typical "image manipulation" operations.  Moreover the sampled random variables are highly dependent on each other.

We follow the Attention for Inference Compilation, by encoding the (input,output) pair into a rich "context".  For each choice, the context is translated into a Query.  Previous choices give rise to (Key, Value) pairs.  The (soft) attention mechanism then provides a way to encode a dependency of a later choice on an earlier one.


Domain Specific Language
------------------------

The DSL has been lifted from the paper "Graphs, Constraints, and Search for the Abstraction and Reasoning Corpus".  It was able to solve a reasonable part of the training set with a small set of abstractions and graph operations.