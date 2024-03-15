# Introduction
This is intended as key/concise advice to those active in developing `ubxlib` and is placed here so that it cannot be missed.

# All That Matters
`ubxlib` exists to facilitate the use of u-blox modules at source-code level, by making it easier for a customer to integrate with current and future u-blox modules; that's it. `ubxlib` has no meaning/value of itself, it is solely a means to that end. Think that way and you will behave in the right way.

# Be The Tail
There is a phrase in English "the tail wagging the dog", used to describe situations where something less important has ended up controlling something rather more important.  `ubxlib` is the tail: a driver for just one or two components in a customers application, it should bend/flex to whatever the customers' application requires, it should NOT place requirements on or limit the customers' application if at all possible.  Examples include:

- all `#define`s that a customer might need to modify should be constructed as `#ifndef/#define/#endif` so that the customer may modify the value without modifying the code,
- functions should not take more than a few 10's of seconds to execute, worst case, otherwise the function must include a `bool keepGoingCallback()` parameter that the customer can use to feed a watchdog timer or give up on that operation if the application is no longer able to block,
- obviously, full and reasonably-commented source code is always available; in the end a customer can always do what they wish.

# APIs Are King
Your code is irrelevant, your API is what matters.  When constructing a new API, write the API function headers, document them in the `.h` file in a Doxygen-friendly way (hint: reading the Doxygen output often gives a useful perspective on what you have achieved), think about the API's dynamic behaviour from an application-writers' perspective (i.e. one who doesn't have the context of the module world), maybe put the whole together with the functions just returning `U_ERROR_COMMON_NOT_IMPLEMENTED` (so no implementation yet but the compilers/checkers will be known to be happy) and submit a PR that can be reviewed **just from the point of view of the API**; don't merge that PR, just use it as a review mechanism.  Only when you and your colleagues are happy with the API is there any point in beginning implementation.

Incidentally, this is also true of setup/automation matters: write the document describing the thing and only _then_ do the thing; this way someone else, or you in a month's time, will be able to replicate it.

Another, equally important way in which a public API is king is that public APIs (i.e. anything in an `api/*.h` file) SHOULD NOT CHANGE, in terms of their form AND, more insidiously, in terms of their dynamic behaviour; this would break the customers' application and the customer will simply go elsewhere, or branch off and not easily be able to take advantage of new/future u-blox modules, should you do it too often.  If, for whatever reason, it becomes necessary to break an API, announce this in advance, deprecating the old and introducing the new with some months gap, and include the words "BREAKING" in capitals at the start of the commit message which causes the breakage so that the commit stands out in the commit history.

# Your Code Doesn't Matter
The only way to build software that will last, can be maintained, is to smother it in fully automated tests.  Once you have a good API with good test coverage, the _implementation_ of the APIs is irrelevant; it has to be there but it is not what matters, what matters is that the API meets the tests.  One _could_ write the tests before the code beneath the APIs, however, given that `ubxlib` is a thin layer between the application and the AT/UBX interface of the module, it is likely that the process of writing the code will have a feedback effect on the shape of the APIs themselves as the behaviour of the module becomes apparent, the "scales will fall from your eyes", so less rework results if you write the code and then follow-up with writing the tests.  All code for a new API should include automated tests, such that a reviewer could review the API and the tests, they don't actually _have_ to review the code (though obviously that would be desirable/natural).

# Test Automation Leads To Speed
Testing must be such that it is run reliably/automagically on every push to this repo.  This allows you to move quickly: write some code, if it is a new API write a test, commit and then push to a branch of this repo and testing should run automatically, without you having to do anything; a green result, or a result that is red for reasons outside your control, reasons that you can justify in a comment against a PR (we test end-to-end and so there may always be intermittent things that will cause a red result) will allow you to proceed safely.  Any chink in this armour will _always_ slow you down later and, with a large code base, you may find it very difficult indeed to recover.

# It Should Always Be Possible To Push To Public `master`
It should always be possible, at any time, to push the `master` branch of the `ubxlib_priv` repo to the `master` branch of the public `ubxlib` repo; the wall of automated-test defence run on every PR makes this possible.  If you have a change which may break a public API, or you have created a new API and you'd like time to settle it down before publishing, take your code via a PR to the `development` branch instead and only merge `development` down to `master` when you are happy that everything on `development` can be made public; PRs to `development` are protected by the same automated-test wall and hence quality is maintained.  As an aside, only EVER do a `merge` of `development` down into `master`, or of `master` up into `development` should `development` need a refresh; NEVER `rebase` or `force-push` as that will cause history to be re-written and break compatiblity with anyone else working on the codeline beyond the point you just vapourized, including the customer.

# Avoid Compile-Time Dependencies Within `ubxlib`
It should be possible for a customers' application to modify anything that you intend to be customer-modifiable by calling a `ubxlib` API function; pins for whatever, timeouts, etc.  Now obviously this is not going to be universally true: e.g. there may be fixed timeouts in some cases made through a `#ifndef/#define/#endif` construction, but the aim should be that we _could_, if we wished, create a pre-compiled library of `ubxlib` and it would be just as usable by the customer.  This maximises flexibility, both for the customer and for us, and, just as important, makes rapid automated testing possible since a single binary may be maintained and used for all tests.

Similarly, conditional compilation should generally be avoided in the `ubxlib` core code: it leads to dead-spots, corners that testing will not cover; it is fine, often unavoidable, to use conditional compilation in the _test_ code, just not in the core code, the stuff beneath an API.

# Document For Next Month's Self
Comment your code as if you were talking to yourself next month, when you've moved on to something else, had the weekend off, maybe had a few drinks and entirely lost the context/cache you once had.  Quite why you took one away from X, ignored a return value, did something out of the ordinary, whatever, will no longer be obvious; you need to say why you did that thing explicitly.

And don't be afraid to add "story"-style comments that guide the reader through the path of the code in a human-readable manner without them having to parse each line like a compiler: you are not a compiler, be kind to your future self (and the customer).

# Your Commit Messages Are Our Only Change Documentation
Similar to commenting, it is easy when writing a commit message to forget that one reading it has absolutely none of your context.  A first line of "fix retry count" is plainly useless, but "fix HTTP retry count" is almost as useless if the change is, say, for cellular only. Something like "cellular fix to HTTP only: increase retry count" would give the correct context: it is a `cell` change, so those only using `ubxlib` for `wifi` can ignore it (if it were a `cell` change only for LARA-R6 then say so and the number of concerned people is reduced even further), it is for the `http` API only, so MQTT users could ignore it, and the count is going up.

Then, in the body of the commit message don't be afraid to say why, in a verbose/readable way, the change is justified; these commit messages are publicly visible and the only change documentation we have so put your back into it.  Note that, for a single commit, a `squash` merge in Github gives you the opportunity to refine/re-word the commit message at the last minute when merging your approved PR.

A corollary of this is that you should try not to combine disparate changes into a single commit; discrete commits are easier for the customer to absorb.

# Do Not Get Fat
Every line of `ubxlib` core code (i.e. ignoring test code) carries a cost for us and for the customer: for the customer it takes precious space in their MCU's memory and for us it increases test time.  Code that improves thread-safety, code that makes a `ubxlib` API easier to use and code which forms a feature that a customer has _requested_, is fine, but don't add code without being _sure_ it is worth it.  Conversely, remove code when you can: deprecate and then remove things that are either no longer supported in the module or that you believe no one is using, despite seeming like a good idea at the time \[a customer can object that they are using the thing in the deprecation period, so do make the deprecation notices clear\].  This especially applies to common code, which cannot be excluded like the `cell`, `gnss`, `wifi` and `ble` code can.

Adding code is putting on weight: make sure it is muscle and not fat.

# Be Careful What You Expose
Only functions/types/#defines that a customer is intended to use should be exposed through the `api` directory.  Anything exposed through the `api` directory must be treated with great care, must only be extended, not broken, etc.

If there are things that another bit of `ubxlib` needs, expose them through a header file named something like `xxx_shared.h` (e.g. [u_geofence_shared.h](/common/geofence/src/u_geofence_shared.h)) and place that header file in the `src` directory, NOT in the `api` directory; the `src` directory is included in the header file search path so the `ubxlib` code will find it but, since only header files from the `api` directory are included in `ubxlib.h`, the customer's code will NOT end up including it by accident.  This gives you freedom to change the functions/types/#defines of that file in future.

Similarly, if there are things that another bit of your own code needs, within the same module, expose that through a header file named `xxx_private.h`, again kept in the `src` directory; no other module of `ubxlib` should include an `xxx_private.h` header file from another module, unless there is a very good reason indeed.  See [common/network/src](/common/network/src) for examples of all of these.

# Keep The Change History Of `master` Clean/Linear
Since the commit messages are our only change documentation, and since Github is not very good at displaying a commit history with merge-commits, it is best, if at all possible, to avoid them and keep a linear commit history on `master`.  This means:

- when merging a PR, try to stick to squash-merges or rebase-merges, rather than plain-old merges of a branch,
- if a customer makes a PR to the public `ubxlib` repo, bring it in as follows:
  - if the customer PR is NEITHER (a) a single commit, NOR (b) made up of nice discrete/sensible changes that would make sense to any other customer, then ask them to squash it into a nice clean single commit and re-push,
  - pull the PR into a branch of `ubxlib_priv` so that you can throw it at the test system to prove that it is all good,
  - make sure that `ubxlib` `master` is up to date with `ubxlib_priv` `master` (i.e. push `ubxlib_priv` `master` to `ubxlib` `master`, which should always be possible, see above),
  - do a rebase-merge of the customer PR into `ubxlib` `master` (i.e. directly, not going via `ubxlib_priv`),
  - pull `ubxlib` `master` back into `ubxlib_priv` `master` (i.e. with the latest `ubxlib_priv` `master` on your machine, pull `ubxlib` `master` and then push that to `ubxlib_priv` `master`).
  
The only exception to the above is when there has been active work on the `ubxlib_priv` `development` branch and that is ready to be brought into `master`: this should be brought into `ubxlib_priv` `master` through a normal (i.e. non-rebase, non-squash) merge since it will likely be a _very_ large commit of disparate things that will not be describable when in one big blob.

As an aside, if `master` moves on underneath a branch **THAT YOU ALONE** are working on, please do a `rebase` of that development branch onto `master`, rather then merging the changes from `master` onto your branch, (i.e. checkout `master` locally, pull the latest `master` and then `rebase` your branch onto `master`); the reason for this is that, otherwise, the merge process can be confused and end up thinking that you intend to remove things that have just been added in the `master` branch.  If you share the branch with someone else, i.e. you are not working on it alone, then take care because rebasing obviously changes history; it may still be the right thing to do, 'cos the ground has indeed moved underneath you, history _has_ changed, but make sure that anyone else who is working on the branch with you is aware of what you have done when you push the branch back to the repo.