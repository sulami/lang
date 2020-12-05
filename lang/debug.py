from lang.conf import DEBUG

def debug(*args):
    if DEBUG:
        print(*args)
