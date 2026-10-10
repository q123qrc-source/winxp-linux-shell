# meadow-fprint-query (phase 4)

A tiny read-only helper the greeter runs to ask fprintd (net.reactivated.Fprint
on the system bus) whether a user has enrolled fingers. It only calls
ListEnrolledFingers, never claims a device, and needs no privileges.
