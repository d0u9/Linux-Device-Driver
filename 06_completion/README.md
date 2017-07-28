# completion

Using completion variables is an easy way to synchronize between two tasks in
the kernel when one task needs to signal to the other that an event has
occurred. One task waits on the completion variable while another task performs
some work. When the other task has completed the work, it uses the completion
variable to wake up any waiting tasks.

---

### ¶ The end

