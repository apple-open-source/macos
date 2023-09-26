from REGRESS import REGRESS_p


if __name__ == "__main__":
    p_suite = REGRESS_p()
    p_suite.run_tests()
    exit(p_suite.rc)
