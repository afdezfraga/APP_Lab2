TARGETS=dotprod_MPI_Sync mxvnm_MPI_Sync dotprod_MPI_Async mxvnm_MPI_Async
MATH_TARGETS=Eval_pi_integral_MPI_Sync Eval_pi_integral_MPI_Async sqrt sqrt_pipeline
MPICC=mpicc

all: $(TARGETS) $(MATH_TARGETS)

$(TARGETS): % : %.c
	$(MPICC) -o $@ $^

$(MATH_TARGETS): % : %.c
	$(MPICC) -o $@ $^ -lm

clean:
	rm -f $(TARGETS) $(MATH_TARGETS) *.o
