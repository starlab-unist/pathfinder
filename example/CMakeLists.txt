include(example.cmake)

# basic examples
example_fuzz_target(check_matmul)
example_fuzz_target(XYZabcd)

# mock deep learning library APIs
example_fuzz_target(conv2d_check_shape)

# exceptional cases
example_fuzz_target(exceptional_nondeterministic)
example_fuzz_target(exceptional_complex)
example_fuzz_target(exceptional_epsilon)
example_fuzz_target(exceptional_unsat)
