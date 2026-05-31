@{
    # The installer intentionally uses Invoke-Expression (the uv bootstrap
    # one-liner) and Write-Host for user-facing progress.
    ExcludeRules = @(
        'PSAvoidUsingInvokeExpression',
        'PSAvoidUsingWriteHost'
    )
}
