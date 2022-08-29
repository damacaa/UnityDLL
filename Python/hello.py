from ast import arg
import numpy as np
import scipy.optimize
import time
from numpy import random
import json

import UnityDLL


def Average(lst):
    return sum(lst) / len(lst)


def g(p, iter, h, m_numDoFs, initialState, targets):
    global lastP
    lastP = p

    steps = []
    current = initialState
    steps.append(initialState)

    for i in range(iter - 1):
        current = UnityDLL.forward(current.x, current.v, p, h)
        steps.append(current)

    g = np.linalg.norm(targets[iter - 1].x - steps[iter - 1].x) ** 2

    return g / iter


def G(p, iter, h, m_numDoFs, initialState, targets, settings):

    steps = [initialState] + UnityDLL.forwardLoop(
        initialState.x, initialState.v, p, settings, h, iter - 1
    )

    G = 0
    for i in range(iter):  # cambiar a steps
        G += np.linalg.norm(targets[i].x - steps[i].x) ** 2
    # G = G / iter

    return (100 / (iter / p.size)) * G


def dGdp(p, iter, h, m_numDoFs, initialState, targets, settings):

    steps = []
    steps.append(initialState)
    steps = [initialState] + UnityDLL.forwardLoop(
        initialState.x, initialState.v, p, settings, h, iter - 1
    )

    _dGdp = np.full(p.size, 0.0)
    _dGdx = []
    _dGdv = []

    for i in range(iter):
        """if i == iter - 1:
            _dGdx.append(2.0 * (targets[iter - 1].x - steps[iter - 1].x))
        else:
            _dGdx.append(np.full(m_numDoFs, 0.0))

        _dGdv.append(np.full(m_numDoFs, 0.0))"""

        _dGdx.append((100 / (iter / p.size)) * 2.0 *
                     (targets[i].x - steps[i].x))

        # _dGdx.append(2.0 * (targets[i].x - steps[i].x))
        _dGdv.append(np.full(m_numDoFs, 0.0))

    i = iter - 2
    while i >= 0:
        backward = UnityDLL.backward(
            steps[i].x,
            steps[i].v,
            steps[i + 1].x,
            steps[i + 1].v,
            p,
            settings,
            _dGdx[i + 1],
            _dGdv[i + 1],
            h,
        )

        _dGdp += backward.dGdp
        _dGdx[i] += backward.dGdx
        _dGdv[i] += backward.dGdv

        i -= 1

    return _dGdp


def Minimize(method="L-BFGS-B", costFunction=G, jacobian=dGdp):

    # scipy.optimize.show_options(solver="minimize", method=method, disp=True)

    data = open(
        "D:/Projects/MassSpringSimulator/UnityProject/Assets/scene.txt", "r"
    ).read()

    data_dict = json.loads(data)

    # PARAMETERS
    iter = data_dict["optimizationIterations"]
    h = data_dict["delta"]

    p = []

    settings = ""

    for o in data_dict["objects"]:

        _settings = o["optimizationSettings"]
        massMode = _settings[0]
        stiffnessMode = _settings[1]

        if massMode == "L":
            p += o["vertMass"]
        elif massMode == "G":
            p += [Average(o["vertMass"])]

        if stiffnessMode == "L":
            p += o["springStiffness"]
        elif stiffnessMode == "G":
            p += [Average(o["springStiffness"])]

        settings += _settings

    desiredParameter = np.array(p)

    # desiredParameter = random.rand(6) + 0.5

    print(
        f"{'-'*60}\nSettings: {settings} Iterations: {iter} Timestep: {h} Target parameter:\n {desiredParameter}\n{'-'*60} "
    )

    # INITIALIZATION
    initialState = UnityDLL.initialize(data, settings)
    m_numDoFs = initialState.x.size

    # CALCULATING TARGET
    targets = [initialState] + UnityDLL.forwardLoop(
        initialState.x,
        initialState.v,
        np.full(0, 0,),
        "nn" * len(data_dict["objects"]),
        h,
        iter - 1,
    )

    p0 = np.full(desiredParameter.size, 0.1)  # initial parameter value
    args = (iter, h, m_numDoFs, initialState, targets, settings)  # extra info
    bnds = [(0.0001, 10000)] * p0.size  # parameter bounds
    options = {"maxiter": 10000, "maxfun": 15000,
               "ftol": 1e-06, "gtol": 1e-06}

    # G(desiredParameter, iter, h, m_numDoFs, initialState, targets)
    # dGdp(desiredParameter, iter, h, m_numDoFs, initialState, targets)

    start = time.time()
    res = scipy.optimize.minimize(
        costFunction,
        p0,
        jac=jacobian,  # jacobian "2-point" "3-point" "cs"
        method=method,
        args=args,
        bounds=bnds,
        options=options,
        callback=ShowProgress,
    )
    end = time.time()

    print(res)

    for i in range(min(10, len(desiredParameter))):
        sign = "+" if (np.sign(res.jac[i]) >= 0) else "-"
        print(round(desiredParameter[i], 4), " --> ",
              round(res.x[i], 4), sign, "\n")

    # print("RESULT:\n", np.round(res.x, 3))
    # print("ERROR\n: ", abs(desiredParameter - res.x))
    print("Time elapsed:\n", str(round((end - start) * 1000.0, 1)), "ms")

    # WRITING NEW FILE
    offset = 0
    for o in data_dict["objects"]:

        settings = o["optimizationSettings"]
        massMode = settings[0]
        stiffnessMode = settings[1]

        nVerts = len(o["vertMass"])
        nSprings = len(o["springStiffness"])

        if massMode in {"L", "l"}:
            o["vertMass"] = res.x[offset: nVerts + offset].tolist()
            offset += nVerts
        elif massMode in {"G", "g"}:
            o["vertMass"] = [res.x[offset]] * nVerts
            offset += 1

        if stiffnessMode in {"L", "l"}:
            o["springStiffness"] = res.x[offset: nSprings + offset].tolist()
            offset += nSprings
        elif stiffnessMode in {"G", "g"}:
            o["springStiffness"] = [res.x[offset]] * nSprings
            offset += 1

    newData = json.dumps(data_dict)
    text_file = open(
        "D:/Projects/MassSpringSimulator/UnityProject/Assets/scene_optimized.txt", "w"
    )
    text_file.write(newData)
    text_file.close()

    log_file = open("C:/debug/log.txt", "a+")
    log_file.write(
        f"{'-'*60}\nSettings: {settings} Iterations: {iter} Timestep: {h} Target parameter: {desiredParameter}\n{'-'*60}\n")
    log_file.write(str(res)+"\n")
    log_file.write("Time elapsed:\n" +
                   str(round((end - start) * 1000.0, 1)) + " ms\n\n")
    log_file.close()

    return res.x


a = 0


def ShowProgress(p):
    global a
    progress = ["-", "/", "|", "\\"]
    print(progress[a], end="\r")
    a += 1
    if a >= len(progress):
        a = 0


Minimize()

# scipy.optimize.show_options(solver="minimize", method="L-BFGS-B", disp=True)
# print(UnityDLL.test())

""" methods = ["CG", "BFGS", "Newton-CG", "L-BFGS-B", "TNC", "SLSQP", "trust-constr"]
methods2 = ["Nelder-Mead", "Powell", "COBYLA"]
for m in methods:
    print(m)
    Minimize(m)
    print("-" * 60) """


# print("-" * 60)

# res2 = fmin_bfgs(G, p0, fprime=dGdp)
# print(res2)
# print("ERROR: ", abs(desiredMass - res2))

# https://gist.github.com/yuyay/3067185
# python -m pip install "d:/Projects/MassSpringSimulator/UnityDLL"
