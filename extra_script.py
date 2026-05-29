Import("env")

producer = env.GetProjectOption("custom_producer_name")
sensor = env.GetProjectOption("custom_sensor_type")
prog_version = env.GetProjectOption("custom_version")

firmware_name = f"firmware_{producer}_v{prog_version}"
env.Replace(PROGNAME=firmware_name)

if sensor == "mock":
    env.Append(CPPDEFINES=["SENSOR_IS_MOCK"])
