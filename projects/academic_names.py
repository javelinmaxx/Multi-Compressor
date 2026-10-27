"""Public academic names, with stable checkout/build identifiers underneath."""
PROJECT_LABELS = {"KuoStereo": "KuoStereo", "ZaoDrive": "ZaoDrive", "XuanPhase": "APRiCOT"}
PUBLIC_PROJECTS = list(PROJECT_LABELS.values())


def public_project(name):
    return PROJECT_LABELS.get(name, name)


def internal_project(name):
    for internal, public in PROJECT_LABELS.items():
        if name in (internal, public):
            return internal
    raise ValueError(f"Unknown academic project: {name}")
