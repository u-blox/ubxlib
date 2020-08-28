#!/usr/bin/env python

'''Select the instances of ubxlib test HW to run.'''

import u_data # Accesses the instance database

# The functions here process a set of file paths that have
# been modified in a pull request and, given a database of
# ubxlib HW instances and knowledge of the ubxlib directory
# structure, decides which ubxlib instances should be run
# to verify that the modifications are good.
#
# The algorithm is as follows:
#
#            Changed                                   Action
#
# (a)    .md/.jpg etc. file                            ignore.
# (b)     platform sdk file          run all instances supporting that platform, that SDK.
# (c)    platform .c .h file         run all instances supporting that platform, one SDK.
# (d) API/implementation *.c *.h file    run all instances supporting that API;
#                                           if just one then return a filter
#                                           string for that one, else run all.
# (e)       any .py file                      run Pylint to check for errors.
# (f)          any                            run Doxygen to check for errors.

# Prefix to put at the start of all prints
PROMPT = "u_select: "

# A list of file extensions to throw away
EXT_DISCARD = ["md", "txt", "jpg", "png", "gitignore"]

# A list of file extensions to keep for code files
EXT_CODE = ["c", "cpp", "h", "hpp"]

# The instances to always run: Lint, Doxygen and AStyle
INSTANCES_ALWAYS = [[0], [1], [2]]

def instances_string(instances):
    '''Return a string of the form "1.2.3, 0.1"'''
    found = ""

    for idx1, instance in enumerate(instances):
        if idx1 > 0:
            found += ", "
        for idx2, item in enumerate(instance):
            if idx2 == 0:
                found += str(item)
            else:
                found += "." + str(item)

    return found


# Perform check (a)
def discard(paths, extensions):
    '''Remove paths with the given extension'''
    wanted = []

    for path in paths:
        include = True
        stripped = path.strip()
        for string in extensions:
            if stripped.endswith(string):
                print("{}ignoring file {}".format(PROMPT, path))
                include = False
        if include:
            wanted.append(stripped)

    return wanted

# Perform check (b)
def instance_sdk(database, paths, instances):
    '''Modify an instance list based on SDKs in paths'''
    instances_local = []

    for path in paths:
        del instances_local[:]
        parts = path.split("/")
        for idx, part in enumerate(parts):
            if (part == "sdk") and (idx > 0) and (idx + 1 < len(parts)):
                platform = parts[idx - 1]
                sdk = parts[idx + 1]
                instances_local.extend(u_data.                            \
                   get_instances_for_platform_sdk(database, platform, sdk)[:])
                if instances_local:
                    print("{}file {} is in SDK {} ({}) implying"           \
                          " instance(s) {}.".format(PROMPT, path, sdk,
                          platform, instances_string(instances_local)))
                    instances.extend(instances_local[:])
                break

# Perform check (c)
def instance_platform(database, paths, extensions, instances):
    '''Modify an instance list based on .c and .h files in platforms'''
    instances_local = []

    for path in paths:
        include = False
        for string in extensions:
            if path.endswith(string):
                include = True
        if include:
            del instances_local[:]
            parts = path.split("/")
            for idx, part in enumerate(parts):
                if (part == "platform") and (idx + 2 < len(parts)):
                    platform = parts[idx + 2]
                    # These are the possible instances but may include
                    # multiple SDKs.
                    possibles = u_data.                                    \
                                get_instances_for_platform(database, platform)
                    #  Add just the first SDK for this platform to the list
                    for possible in possibles:
                        if (len(possible) == 1) or (possible[1] == 0):
                            instances_local.append(possible[:])
                    # We don't need more then one SDK for a .c or .h
                    # file so check if the platform is already present in
                    # instances for another SDK of this platform
                    included = False
                    for instance in instances:
                        if platform.lower() == u_data.                     \
                        get_platform_for_instance(database, instance).lower():
                            included = True
                            break
                    if not included:
                        # If it is not already included, include
                        # the SDK 0 instances for this platform
                        if instances_local:
                            print("{}file {} is in platform {} implying"   \
                                  " instance(s) {}.".format(PROMPT, path,
                                  platform, instances_string(instances_local)))
                            instances.extend(instances_local[:])
                    else:
                        print("{}file {} is in platform {}, an instance"    \
                              " of which is already included.".
                              format(PROMPT, path, platform))
                    break

# Perform check (d)
def instance_api(database, paths, extensions, instances):
    '''Modify an instance list based on .c and .h files in APIs'''
    instances_local = []
    api_saved = None
    more_than_one = False

    for path in paths:
        include = False
        for string in extensions:
            if path.endswith(string):
                include = True
        if include:
            del instances_local[:]
            parts = path.split("/")
            for idx, part in enumerate(parts):
                if (part in ("api", "src", "test")) and (idx > 0):
                    api = parts[idx - 1]
                    instances_local.extend(u_data.get_instances_for_api(database, api)[:])
                    if instances_local:
                        print("{}file {} is in API \"{}\" implying"    \
                              " instance(s) {}.".format(PROMPT, path,
                              api, instances_string(instances_local)))
                        instances.extend(instances_local[:])
                    if api_saved and (api != api_saved):
                        more_than_one = True
                    api_saved = api
                    break

    # If the change is in more than one API, don't
    # return an API string
    if more_than_one:
        api_saved = None

    return api_saved

# Perform all checks, update instances and return a filter
def select(database, instances, paths):
    '''Process a set of file paths and return the instances to run'''
    interesting = []
    instances_local = []
    dedup = []
    filter_string = None

    print("{}selecting what instances to run based on {} file(s)...".
          format(PROMPT, len(paths)))
    for idx, path in enumerate(paths):
        print("{}file {}: {}".format(PROMPT, idx + 1, path))

    # First throw away any file paths known to be uninteresting
    interesting = discard(paths, EXT_DISCARD)

    # Add the platforms that must be run because
    # an interesting file is in an SDK directory of a
    # platform
    instance_sdk(database, interesting, instances_local)

    # Add the platforms that must be run because
    # a file with an "EXT_CODE" extension is in a
    # [non-SDK] directory of a platform
    instance_platform(database, interesting,
                      EXT_CODE, instances_local)

    # Add the instances that must be run because a file
    # path includes a file in an API that an instance uses
    filter_string = instance_api(database, interesting,
                                 EXT_CODE, instances_local)

    # Check if PyLint needs to be run
    print("{}checking if pylint needs to be run...".format(PROMPT))
    for py_file in interesting:
        if py_file.endswith(".py"):
            instances_local.append([3])
            break

    # Add any instances that must always be run
    print("{}adding instances that are always run...".format(PROMPT))
    instances_local.extend(INSTANCES_ALWAYS[:])

    # Create a de-duplicated list
    for instance in instances_local:
        if instance not in dedup:
            dedup.append(instance[:])

    # Append to the list that was passed in
    dedup.sort()
    instances.extend(dedup[:])

    print("{}final instance list: {}".format(PROMPT, instances_string(dedup), end=""))
    if filter_string:
        print("with filter \"{}\".".format(filter_string, end=""))
    print()

    return filter_string
