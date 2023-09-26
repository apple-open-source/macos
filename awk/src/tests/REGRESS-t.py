from REGRESS import REGRESS_t


if __name__ == "__main__":
    t_suite = REGRESS_t()
    t_suite.run_tests()
    exit(t_suite.rc)
