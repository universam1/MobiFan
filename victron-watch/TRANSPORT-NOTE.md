# Why this directory is here

This is a **mirror** of the standalone project
[universam1/Victron-MPPT-SmartWatch-Monitor-](https://github.com/universam1/Victron-MPPT-SmartWatch-Monitor-).

It was committed here only because this session had read-only GitHub access to that
repository — `git push` and the contents API both answered
`403 Resource not accessible by integration` — and the work should not be lost together
with the container.

To move it into its own repository:

```sh
git subtree split --prefix=victron-watch -b victron-monitor
git push git@github.com:universam1/Victron-MPPT-SmartWatch-Monitor-.git victron-monitor:main
```

Then delete this directory here.
