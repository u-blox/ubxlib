#!/usr/bin/env python

'''Select the instances of ubxlib test HW to run.'''

from scripts import u_data # Accesses the instance database

# The functions here process a set of file paths that have
# been modified in a pull request and, given a database of
# ubxlib HW instances and knowledge of the ubxlib directory
# structure, decides which ubxlib instances should be run
# to verify that the modifications are good.
#
# The algorithm is as follows:
#
#            Changed                                     Action
#
# (a)    .md/.jpg etc. file                              ignore.
# (b)   platform-specific file             run all instances supporting that platform.
# (c) API/implementation *.c *.h file    run all instances supporting that API (according
#                                           to DATABASE.md); if just one then return
#                                              a filter string for that one.
# (d)     any .py file                       run Pylint to check for errors.
# (e)         any                            run Doxygen to check for errors.
#
# In addition to all of the above a quite separate check
# is provided: do any of the file paths that have been
# changed affect the ubxlib test automation.  This allows
# the caller to determine if those files need to be updated.

# Prefix to put at the start of all prints
PROMPT = "u_select: "

# A list of file extensions to throw away
EXT_DISCARD = ["md", "txt", "jpg", "png", "gitignore"]

# A list of file names that should never be discarded,
# despite their extensions
NEVER_DISCARD = ["DATABASE.md", "CMakeLists.txt", "source.txt", "include.txt"]

# A list of file extensions to keep for code files
EXT_CODE = ["c", "cpp", "h", "hpp"]

# The instances to always run: CodeChecker, Doxygen, AStyle, size, ubxlib.h and malloc checking
INSTANCES_ALWAYS = [[1], [2], [4], [5], [6,1], [6,2], [7], [8], [9]]

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
def discard(paths, extensions, exceptions):
    '''Remove paths with the given extensions unless excepted'''
    wanted = []

    for path in paths:
        include = True
        stripped = path.strip()
        for string in extensions:
            excepted = False
            for string2 in exceptions:
                if stripped.endswith(string2):
                    excepted = True
                    break
            if not excepted and stripped.endswith(string):
                print(f"{PROMPT}ignoring file {path}")
                include = False
        if include:
            wanted.append(stripped)

    return wanted

# Perform check (b)
def instance_platform(database, paths, instances):
    '''Modify an instance list based on a file being specific to a given platform'''
    instances_local = []

    for path in paths:
        del instances_local[:]
        got_platform = False
        platform = None
        got_mcu = False
        mcu = None
        parts = path.split("/")
        for idx, part in enumerate(parts):
            if part == "platform":
                got_platform = True
            else:
                if got_platform and (platform is None):
                    if platform == "common":
                        # Not a platform, common code
                        break
                    platform = part
                else:
                    if got_platform and (platform is not None):
                        # Keep checking to see if the file is actually
                        # MCU-specific
                        if got_platform and (platform is not None) and (part == "mcu"):
                            got_mcu = True
                        else:
                            if got_platform and (platform is not None) and got_mcu and (mcu is None):
                                mcu = part
                            else:
                                if got_platform and (platform is not None) and got_mcu and \
                                   (mcu is not None):
                                    # We have an MCU-specific file
                                    toolchains = u_data.                                          \
                                      get_toolchains_for_platform_mcu(database, platform, mcu)
                                    if (toolchains is None) or (idx + 1 == len(parts)):
                                        # A file in the <mcu> directory or a sub-directory
                                        # of a platform which doesn't support a choice of
                                        # toolchains, so it can be added
                                        instances_local.extend(u_data.                            \
                                          get_instances_for_platform_mcu_toolchain(database,
                                                                                   platform,
                                                                                   mcu, None)[:])
                                        if instances_local:
                                            print(f"{PROMPT}file {path} is in platform/MCU"
                                                  f" {platform}/{mcu} implying"   \
                                                  f" instance(s) {instances_string(instances_local)}.")
                                            instances.extend(instances_local[:])
                                        break
                                    # The path might be in a sub-directory for a specific
                                    # toolchain so check for that
                                    for toolchain in toolchains:
                                        if toolchain.lower() == part:
                                            instances_local.extend(u_data.                 \
                                               get_instances_for_platform_mcu_toolchain(database, \
                                                                                        platform, \
                                                                                        mcu,     \
                                                                                        toolchain)[:])
                                            if instances_local:
                                                print("{}file {} is in platform/MCU/toolchain "
                                                      " {}/{}/{} implying instance(s) {}.".           \
                                                      format(PROMPT, path, platform, mcu, toolchain, \
                                                      instances_string(instances_local)))
                                                instances.extend(instances_local[:])
                                            break
                                    break
        if got_platform and (platform is not None) and (mcu is None):
            if platform == "common":
                # Common code, best run the lot
                print(f"{PROMPT}file {path} is in common code need to do the lot.")
                instances.extend(u_data.get_instances_all(database)[:])
            else:
                # Something under the platform directory with no MCU directory: since
                # we can't be sure about the file extensions used by the various gubbins
                # underneath a platform we have to assume that it is a
                # significant change
                instances_for_platform = u_data.                                  \
                   get_instances_for_platform_mcu_toolchain(database, platform, \
                                                            None, None)[:]
                if instances_for_platform:
                    print(f"{PROMPT}file {path} is in platform {platform} implying"        \
                          f" instance(s) {instances_string(instances_for_platform)}.")
                    instances.extend(instances_for_platform[:])
                else:
                    # Doesn't even match a known platform: do the lot
                    print(f"{PROMPT}file {path} is not in a known platform,"    \
                          " need to do the lot.")
                    instances.extend(u_data.get_instances_all(database)[:])
            break

# Perform check (d)
def instance_api(database, paths, extensions, instances):
    '''Modify an instance list based on .c and .h files in APIs'''
    instances_local = []
    api_saved = None
    run_everything = False
    got_platform = False
    got_api_src_or_test = False

    for path in paths:
        include = False
        for string in extensions:
            if path.endswith(string):
                include = True
        if include and not run_everything:
            del instances_local[:]
            parts = path.split("/")
            for idx, part in enumerate(parts):
                # Check for an api, src or test directory
                # but not one hanging off a platform
                # directory
                if part == "platform":
                    got_platform = True
                    break
                if (part in ("api", "src", "test")) and (idx > 0):
                    got_api_src_or_test = True
                    if u_data.api_in_database(database, parts[idx - 1]):
                        api = parts[idx - 1]
                        instances_local.extend(u_data.get_instances_for_api(database, api)[:])
                        if instances_local:
                            print(f"{PROMPT}file {path} is in API \"{api}\" implying"    \
                                  f" instance(s) {instances_string(instances_local)}.")
                            instances.extend(instances_local[:])
                        if api_saved and (api != api_saved):
                            run_everything = True
                            print("{}files are in more than one API so,"    \
                                  " can't filter, need to run the lot.".     \
                                  format(PROMPT))
                            break
                        api_saved = api
                        break
                    # The .c/.h file is in an api/src/test directory
                    # that isn't included in the database for filtering
                    # so have to run the lot
                    run_everything = True
                    print("{}file {} is under API {} but this is not,"    \
                          " in the database so can't filter,"             \
                          " need to run the lot.".     \
                          format(PROMPT, path, parts[idx - 1]))
                    break
            if not got_platform and not got_api_src_or_test:
                # The .c/.h file is not in platform and not under an api/src/test
                # directory, so can't filter
                print(f"{PROMPT}file {path} is not under an API or platform,"  \
                      " need to run the lot.")
                run_everything = True
                break

    # If the change is in more than one API, or is outside the
    # API or platform, return an API string indicating everything
    if run_everything:
        api_saved = "*"

    return api_saved

# Convert blah_blah to blahBlah
def snake_to_camel(snake):
    '''Convert a snake-case string to a camel-case string'''
    camel = snake

    if snake:
        camel = ""
        upper = False
        for character in snake:
            if character != "_":
                if upper:
                    camel += character.upper()
                    upper = False
                else:
                    camel += character
            else:
                upper = True

    return camel

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
        print(f"{PROMPT}file {idx + 1}: {path}")

    # First throw away any file paths known to be uninteresting
    interesting = discard(paths, EXT_DISCARD, NEVER_DISCARD)

    # Add the instances that must be run because
    # an interesting file is platform-specific
    instance_platform(database, interesting, instances_local)

    # Add the instances that must be run because a file
    # path includes a file in an API that an instance uses
    filter_string = instance_api(database, interesting,
                                 EXT_CODE, instances_local)

    # The filter string will use snake case but the test names
    # are camel case so convert it
    filter_string = snake_to_camel(filter_string)

    # If the filter string is a wildcard, add everything
    if filter_string == "*":
        instances_local.extend(u_data.get_instances_all(database)[:])
        filter_string = None

    # Check if PyLint needs to be run
    print(f"{PROMPT}checking if pylint needs to be run...")
    for py_file in interesting:
        if py_file.endswith(".py"):
            instances_local.append([3])
            break

    # Add any instances that must always be run
    print(f"{PROMPT}adding instances that are always run...")
    instances_local.extend(INSTANCES_ALWAYS[:])

    # Create a de-duplicated list
    for instance in instances_local:
        if instance not in dedup:
            dedup.append(instance[:])

    # Append to the list that was passed in
    dedup.sort()
    instances.extend(dedup[:])

    print("{}final instance list: {}".format(PROMPT, instances_string(dedup)), end="")
    if filter_string:
        print(" with filter \"{}\".".format(filter_string), end="")
    print()

    return filter_string

def automation_changes(paths):
    '''Return true if any automation files are included in paths'''
    is_changed = False

    if paths:
        for path in paths:
            if path.startswith("port/platform/common/automation/") and \
               (path.lower().endswith(".py") or path.lower().endswith("database.md") or \
                path.lower().endswith("jenkinsfile")):
                is_changed = True
                break

    return is_changed
