import os

# Stub for rdar://49489227
if not os.getenv("PYTHON_RUNNING_REGRTEST"):
    import recertifi.implementation
